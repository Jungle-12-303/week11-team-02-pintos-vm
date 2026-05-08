# Pintos VM 로드맵: `load_segment()`부터 페이지 생성과 코드 로딩까지

## 이 문서의 목적
이 문서는 `pintos/userprog/process.c`의 `load_segment()`부터 시작해서,

- 실행 파일의 세그먼트가 어떻게 "페이지 단위 계획"으로 쪼개지는지
- 왜 Project 3에서는 즉시 파일을 읽지 않고 `uninit page`만 등록하는지
- 실제 코드는 언제 메모리에 올라오는지
- page fault가 났을 때 어떤 함수들이 이어서 호출되는지

를 한 흐름으로 정리한 로드맵이다.

기준 파일:
- `pintos/userprog/process.c`
- `pintos/userprog/exception.c`
- `pintos/vm/vm.c`
- `pintos/vm/uninit.c`
- `pintos/vm/anon.c`
- `pintos/vm/file.c`

---

## 먼저 큰 그림

Project 2에서는 `load_segment()`가 곧바로
- 물리 페이지를 하나 할당하고
- 파일에서 읽고
- 0으로 채우고
- page table에 바로 매핑했다.

Project 3 VM에서는 방식이 바뀐다.

`load_segment()`는 더 이상 "즉시 로드"를 하지 않고,
"이 주소에 나중에 이런 페이지가 필요하다"는 약속만 `SPT`에 등록한다.

즉, 흐름이 이렇게 바뀐다.

```text
Project 2
load_segment()
  -> palloc_get_page()
  -> file_read()
  -> install_page()
  -> 끝

Project 3
load_segment()
  -> vm_alloc_page_with_initializer()
  -> SPT에 uninit page 등록
  -> 끝

나중에 첫 접근 시
page fault
  -> vm_try_handle_fault()
  -> vm_claim_page()
  -> vm_do_claim_page()
  -> uninit_initialize()
  -> lazy_load_segment()
  -> 실제 file_read()
```

핵심은:
- `load_segment()`는 "로드 예약"
- `page fault`는 "실제 로드 실행"

---

## 전체 호출 흐름

```text
process_exec()
  -> load()
    -> ELF header / program header 분석
    -> PT_LOAD segment마다 load_segment()
      -> 페이지별 aux 준비
      -> vm_alloc_page_with_initializer()
        -> struct page 생성
        -> uninit_new()
        -> SPT 삽입
    -> setup_stack()
    -> 유저 진입 (do_iret)

유저 코드가 아직 안 올라온 주소 접근
  -> page_fault() in exception.c
    -> vm_try_handle_fault()
      -> spt_find_page()
      -> vm_claim_page()
        -> vm_do_claim_page()
          -> vm_get_frame()
          -> pml4_set_page()
          -> swap_in(page, frame->kva)
            -> 현재 page가 uninit이면 uninit_initialize()
              -> page_initializer(...)   // anon_initializer or file_backed_initializer
              -> init(page, aux)         // lazy_load_segment()
                -> file_seek/file_read/memset
          -> 복귀
    -> fault 처리 성공, 유저 코드 계속 실행
```

---

## 1. `load()`에서 `load_segment()`가 호출되기 전

위치는 `pintos/userprog/process.c`의 `load()`이다.

여기서 하는 일:
- ELF 헤더 읽기
- 프로그램 헤더(`Phdr`) 순회
- `PT_LOAD`인 세그먼트만 골라냄
- 각 세그먼트를 page 단위로 나눠서 `load_segment()`에 넘김

중요 계산:
- `file_page`: 파일에서 읽기 시작할 페이지 정렬 오프셋
- `mem_page`: 유저 가상 주소의 페이지 시작 주소
- `page_offset`: 첫 페이지 안에서 실제 데이터가 시작하는 오프셋
- `read_bytes`: 파일에서 읽어야 하는 총 바이트 수
- `zero_bytes`: 메모리에서 0으로 채워야 하는 총 바이트 수

즉, `load()`는
"이 세그먼트를 어느 가상 주소에, 몇 페이지로, 읽기/쓰기 가능 여부를 어떻게 해서 올릴 것인가"
를 계산해서 `load_segment()`에 전달하는 단계다.

---

## 2. `load_segment()`의 역할

위치는 `pintos/userprog/process.c`의 VM 버전 `load_segment()`이다.

현재 코드의 핵심 루프:

```c
while (read_bytes > 0 || zero_bytes > 0) {
    size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
    size_t page_zero_bytes = PGSIZE - page_read_bytes;

    void *aux = NULL;
    if (!vm_alloc_page_with_initializer (VM_ANON, upage, writable,
                                         lazy_load_segment, aux))
        return false;

    read_bytes -= page_read_bytes;
    zero_bytes -= page_zero_bytes;
    upage += PGSIZE;
}
```

여기서 중요한 포인트는 두 개다.

### 2-1. 이 함수는 페이지마다 "설계도"를 만든다
페이지 하나마다 다음 정보가 필요하다.
- 어떤 파일에서 읽을지
- 파일의 어느 오프셋부터 읽을지
- 몇 바이트를 읽고 나머지를 몇 바이트 0으로 채울지
- writable인지

이 정보가 바로 `aux`로 넘어가야 한다.

보통 별도 구조체를 하나 만든다. 예:

```c
struct lazy_load_arg {
    struct file *file;
    off_t ofs;
    size_t page_read_bytes;
    size_t page_zero_bytes;
};
```

그리고 각 페이지마다:
- `aux`를 동적 할당
- `file`, `ofs`, `page_read_bytes`, `page_zero_bytes` 저장
- `vm_alloc_page_with_initializer()`에 넘김

### 2-2. 현재 템플릿은 `VM_ANON`으로 되어 있지만, 실행 파일 lazy load 의도는 `uninit -> initializer -> 실제 내용 채우기`이다
이 코드베이스 템플릿 구조상 `vm_alloc_page_with_initializer()`는
먼저 `uninit page`를 만들고,
첫 fault 때 `anon_initializer()` 또는 `file_backed_initializer()` 같은 타입별 initializer를 태운다.

실행 파일 세그먼트 lazy load는 보통:
- 페이지 객체는 일단 `uninit`
- 첫 fault 시 적절한 페이지 타입으로 전환
- `lazy_load_segment()`가 파일 내용을 읽어 넣음

형태로 이해하면 된다.

즉 `load_segment()`는 실제 파일 read를 하지 않는다.

---

## 3. `vm_alloc_page_with_initializer()`에서 실제로 만들어지는 것

위치는 `pintos/vm/vm.c`.

이 함수의 역할:
- 현재 스레드의 `spt`에서 같은 `upage`가 이미 있는지 확인
- 새 `struct page`를 만들기
- 이 page를 당장은 `uninit page`로 등록
- 나중에 첫 fault가 났을 때 어떤 initializer를 탈지 저장
- `spt_insert_page()`로 supplemental page table에 넣기

의도 흐름은 아래와 같다.

```text
vm_alloc_page_with_initializer(type, upage, writable, init, aux)
  -> page 구조체 malloc
  -> type에 맞는 page_initializer 선택
     - anon_initializer
     - file_backed_initializer
  -> uninit_new(page, upage, init, type, aux, page_initializer)
  -> writable 같은 메타데이터 저장
  -> spt_insert_page(spt, page)
```

여기서 `uninit_new()`가 매우 중요하다.

---

## 4. `uninit_new()`가 의미하는 것

위치는 `pintos/vm/uninit.c`.

`uninit_new()`는 page를 아직 실체가 없는 "pending page" 상태로 만든다.

저장되는 정보:
- `page->operations = &uninit_ops`
- `page->va = upage`
- `page->frame = NULL`
- `page->uninit.init = lazy_load_segment`
- `page->uninit.type = 실제로 가야 할 타입`
- `page->uninit.aux = aux`
- `page->uninit.page_initializer = anon_initializer` 또는 `file_backed_initializer`

즉 이 순간 page는 아직:
- 물리 frame이 없음
- page table 매핑도 없음
- 파일 내용도 안 읽음

하지만 나중에 첫 접근이 오면 어떻게 초기화할지 정보는 모두 들고 있다.

---

## 5. SPT에 들어간 뒤 유저 프로그램은 바로 실행된다

`load()`는 세그먼트 페이지들을 모두 SPT에 등록한 뒤:
- `setup_stack()`
- entry point (`if_->rip`) 설정
- user stack argument 세팅
- `do_iret()`로 유저 모드 진입

여기서 중요한 점:
- 유저 코드의 가상 주소는 존재하는 것처럼 보이지만
- 실제 물리 메모리는 아직 대부분 연결되지 않았을 수 있다

그래서 첫 instruction fetch 또는 첫 data access 때 page fault가 날 수 있다.

이게 정상이다.

---

## 6. 첫 접근 시 `page_fault()`가 시작점

위치는 `pintos/userprog/exception.c`.

흐름:

```c
fault_addr = (void *) rcr2();
not_present = (f->error_code & PF_P) == 0;
write = (f->error_code & PF_W) != 0;
user = (f->error_code & PF_U) != 0;

if (vm_try_handle_fault (f, fault_addr, user, write, not_present))
    return;
```

여기서 `vm_try_handle_fault()`가 성공하면
이 fault는 "버그"가 아니라 "lazy page를 이제 실제로 올리면 되는 상황"이라는 뜻이다.

---

## 7. `vm_try_handle_fault()`가 해야 하는 판단

위치는 `pintos/vm/vm.c`.

이 함수의 일반적인 역할:

1. fault가 처리 가능한 fault인지 검사
- 커널 주소 접근인지
- write-protected page인지
- stack growth 대상인지

2. `spt_find_page()`로 fault address의 페이지를 찾기
- `pg_round_down(addr)` 기준으로 page lookup

3. page가 있으면 `vm_do_claim_page(page)`
- 아직 없는 page인데 stack 조건을 만족하면 `vm_stack_growth(addr)` 후 claim

즉 이 함수는
"이 fault를 고칠 수 있는가?"
를 판단하는 디스패처 역할이다.

---

## 8. `vm_claim_page()` / `vm_do_claim_page()`에서 실제 매핑이 만들어진다

위치는 `pintos/vm/vm.c`.

의도 흐름:

```text
vm_claim_page(va)
  -> spt_find_page()
  -> vm_do_claim_page(page)

vm_do_claim_page(page)
  -> vm_get_frame()
  -> frame <-> page 연결
  -> pml4_set_page(page->va, frame->kva, writable)
  -> swap_in(page, frame->kva)
```

여기서 중요한 지점:

### 8-1. frame 확보
- 여유 frame이 있으면 새로 할당
- 없으면 victim을 골라 evict

### 8-2. page table 연결
- `page->va`가 이제 특정 `frame->kva`와 연결됨
- 아직 내용은 안 채워졌을 수 있음

### 8-3. `swap_in(page, frame->kva)` 호출
이 이름 때문에 헷갈릴 수 있는데,
여기서 `swap_in()`은 꼭 "swap disk에서만 읽는다"는 뜻이 아니다.

현재 page type의 `operations->swap_in`을 호출한다.

즉:
- `uninit page`면 `uninit_initialize()`
- `anon page`면 `anon_swap_in()`
- `file page`면 `file_backed_swap_in()`

이렇게 type-dispatch가 일어난다.

---

## 9. 첫 fault에서는 `uninit_initialize()`가 실행된다

위치는 `pintos/vm/uninit.c`.

현재 구현 구조:

```c
return uninit->page_initializer (page, uninit->type, kva) &&
       (init ? init (page, aux) : true);
```

순서상 의미:

1. `page_initializer(...)`
- page를 실제 타입의 page로 바꾼다
- 예: `anon_initializer()` 또는 `file_backed_initializer()`

2. `init(page, aux)`
- 여기서는 보통 `lazy_load_segment()`
- 실제 파일 내용을 읽거나 0으로 채움

즉 `uninit_initialize()`는
"page 객체의 타입 전환"과 "실제 내용 채우기"를 이어주는 관문이다.

---

## 10. `lazy_load_segment()`에서 진짜 코드/데이터가 메모리에 올라온다

위치는 `pintos/userprog/process.c`.

이 함수는 템플릿 상태지만, 의도는 명확하다.

해야 하는 일:
- `aux`에서
  - `file`
  - `ofs`
  - `page_read_bytes`
  - `page_zero_bytes`
  를 꺼냄
- `file_seek(file, ofs)`
- `file_read(file, kva, page_read_bytes)`
- `memset(kva + page_read_bytes, 0, page_zero_bytes)`
- 사용한 `aux` 해제

즉 실제 디스크 I/O는 여기서 발생한다.

이 지점이 바로
"코드가 실제 메모리에 로드되는 순간"
이다.

---

## 11. 그래서 실행 파일 코드 한 페이지는 실제로 언제 올라오나?

예를 들어 entry point가 어떤 code page 안에 있다고 하자.

흐름:

```text
load()
  -> load_segment()
  -> 그 code page를 SPT에 uninit page로만 등록

do_iret()
  -> 유저 모드 시작
  -> CPU가 entry point instruction fetch 시도
  -> 해당 가상 주소에 아직 실제 매핑 없음
  -> page fault

page_fault()
  -> vm_try_handle_fault()
  -> vm_do_claim_page()
  -> uninit_initialize()
  -> lazy_load_segment()
  -> file에서 code bytes 읽음
  -> page table 매핑 완료
  -> 유저 instruction 재시도
```

즉 실행 파일의 코드 페이지조차도
"프로세스 시작 전에 전부 올라오는 게 아니라, 실제 실행 직전에 fault로 올라온다"
가 VM의 핵심이다.

---

## 12. 스택은 왜 `load_segment()` 흐름과 조금 다른가?

스택은 실행 파일 세그먼트가 아니라 별도로 `setup_stack()`에서 준비한다.

현재 VM 버전 `setup_stack()`의 의도:
- `USER_STACK - PGSIZE` 위치에 stack page 하나를 준비
- `vm_alloc_page(...)` 또는 `vm_alloc_page_with_initializer(...)`
- 바로 `vm_claim_page()` 해서 첫 스택 페이지는 즉시 사용할 수 있게 함
- `if_->rsp = USER_STACK`

즉 스택은 보통:
- 첫 1페이지는 즉시 준비
- 그 아래로 더 필요해지면 `vm_try_handle_fault()` -> `vm_stack_growth()`로 확장

이라는 별도 흐름을 탄다.

---

## 13. 이 코드베이스에서 구현할 핵심 자료구조

### `struct page`
필요한 메타데이터 예시:
- `writable`
- `struct hash_elem` 또는 `list_elem`
- 필요하다면 stack marker

### `struct supplemental_page_table`
보통 hash 기반으로 구현:
- key: user virtual page address
- value: `struct page *`

### `lazy load aux`
실행 파일 lazy loading용 구조체:

```c
struct lazy_load_arg {
    struct file *file;
    off_t ofs;
    size_t page_read_bytes;
    size_t page_zero_bytes;
};
```

### anon/file page 메타데이터
나중에 swap, mmap까지 생각하면:
- anon page: swap slot index
- file page: backing file, ofs, read_bytes, zero_bytes, writable, dirty 관련 정보

---

## 14. 구현 순서 제안

### 1단계: `load_segment()` 경로만 먼저 완성
- `lazy_load_arg` 정의
- `load_segment()`에서 페이지별 aux 생성
- `vm_alloc_page_with_initializer()` 구현
- `spt_find_page`, `spt_insert_page` 구현

이 단계 목표:
- 세그먼트 페이지들이 SPT에 등록됨

### 2단계: fault로 실제 로드 연결
- `vm_claim_page()`
- `vm_do_claim_page()`
- `vm_get_frame()`
- `lazy_load_segment()`
- `vm_try_handle_fault()`

이 단계 목표:
- 첫 instruction fetch 때 code page가 정상 로드됨

### 3단계: stack
- `setup_stack()`
- `vm_stack_growth()`

### 4단계: protection
- read-only page write 차단
- invalid access kill

### 5단계: swap / mmap 확장
- anon swap
- file-backed page
- `do_mmap()` / `do_munmap()`

---

## 15. 디버깅할 때 꼭 확인할 체크포인트

### `load_segment()` 직후
- page table에는 아직 user page가 없어도 됨
- 대신 SPT에는 page가 있어야 함

### 첫 page fault 시
- `fault_addr`가 기대한 code/data 주소인지
- `spt_find_page()`가 NULL이 아닌지

### `vm_do_claim_page()` 직후
- `page->frame != NULL`
- `pml4_get_page()`가 정상 주소를 반환하는지

### `lazy_load_segment()` 직후
- frame 내용의 앞부분이 파일 내용과 맞는지
- 나머지 영역이 0으로 채워졌는지

### 유저 재실행 후
- 같은 주소에 대해 같은 fault가 무한 반복되지 않는지

---

## 16. 한 줄 요약

`load_segment()`는 페이지를 "즉시 로드"하는 함수가 아니라,  
"이 가상 페이지는 나중에 첫 접근이 오면 `lazy_load_segment()`로 채워라"라고 SPT에 등록하는 함수다.

실제 코드/데이터가 메모리에 올라오는 순간은 `page_fault()` 이후의  
`vm_try_handle_fault()` -> `vm_do_claim_page()` -> `uninit_initialize()` -> `lazy_load_segment()` 체인 안이다.
