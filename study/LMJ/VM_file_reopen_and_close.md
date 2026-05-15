# Pintos VM 정리: 왜 `file_reopen()`을 해야 하고 어디서 `file_close()`해야 하는가

## 이 문서의 목적
이 문서는 Pintos VM에서 실행 파일 세그먼트를 lazy loading 할 때

- 왜 `load_segment()`에서 원본 `file`을 그대로 쓰면 안 되는지
- 왜 각 페이지용 `aux`에 `file_reopen(file)` 결과를 넣어야 하는지
- `lazy_load_segment()`에서 왜 `file_close()`를 해줘야 하는지
- `free()`와 `file_close()`의 순서가 왜 중요한지

를 이해하기 쉽게 정리한 자료이다.

기준 파일:
- `pintos/userprog/process.c`
- `pintos/filesys/file.c`
- `pintos/vm/uninit.c`
- `pintos/vm/vm.c`

---

## 먼저 결론

lazy loading에서는 `load()` 시점에 파일을 바로 읽지 않는다.
대신 "나중에 페이지 폴트가 나면 이 파일의 이 오프셋에서 읽어라"라는 정보만 저장해 둔다.

그래서 `load_segment()` 안에서 사용하는 파일 포인터는
"지금만 잠깐 살아 있으면 되는 포인터"가 아니라
"미래의 페이지 폴트 시점까지 안전하게 살아 있어야 하는 포인터"여야 한다.

이 때문에:

- `aux->file = file;` 만 하면 위험하다.
- `aux->file = file_reopen(file);` 로 독립 참조를 만들어 줘야 한다.
- 그 페이지 로딩이 끝나면 `lazy_load_segment()`에서 `file_close(aux->file);`를 해줘야 한다.

---

## 큰 흐름부터 보기

실행 파일 lazy loading의 흐름은 대략 이렇다.

```text
load()
  -> filesys_open() 또는 이미 열린 실행 파일 사용
  -> load_segment()
    -> 페이지마다 aux 생성
    -> aux에 file, ofs, read_bytes, zero_bytes 저장
    -> vm_alloc_page_with_initializer(...)
    -> 실제 파일 read는 아직 안 함

나중에 유저 코드가 해당 주소에 처음 접근
  -> page fault
  -> vm_try_handle_fault()
  -> vm_claim_page()
  -> vm_do_claim_page()
  -> swap_in()
  -> uninit_initialize()
  -> lazy_load_segment(page, aux)
    -> file_read_at(aux->file, ...)
    -> memset(...)
    -> file_close(aux->file)
    -> free(aux)
```

핵심은:

- `load_segment()`는 "지금 읽는 함수"가 아니다.
- `lazy_load_segment()`가 "나중에 실제로 읽는 함수"다.

즉 `aux` 안의 파일 포인터는 미래 시점까지 살아 있어야 한다.

---

## `file_open`, `file_reopen`, `file_close`의 역할

### `file_open`

`inode`를 받아서 `struct file` 객체를 새로 만든다.

쉽게 말하면:
- 실제 파일 본체는 inode가 대표한다.
- `file_open()`은 그 파일을 다룰 수 있는 "열린 파일 핸들"을 만든다.

보통 `struct file` 안에는:
- 어느 inode를 보는지
- 현재 파일 위치 `pos`
- deny write 상태 같은 메타데이터

가 들어간다.

---

### `file_reopen`

이미 열린 `struct file *file`을 기반으로
같은 실제 파일을 가리키는 새로운 `struct file` 핸들을 하나 더 만든다.

쉽게 비유하면:
- `file_open`: 손잡이 1개 만들기
- `file_reopen`: 같은 문에 대한 손잡이 하나 더 복제하기

중요한 점:
- 같은 실제 파일을 본다.
- 하지만 "열린 파일 객체"는 별개다.
- 하나를 닫아도 다른 하나는 계속 살아 있을 수 있다.

---

### `file_close`

더 이상 필요 없는 `struct file` 핸들을 닫는다.

이건 파일 내용을 지우는 게 아니라:
- 이 열린 파일 객체의 사용을 끝내고
- 내부 참조를 정리하고
- 필요하면 메모리를 해제하는 동작이다.

즉:
- `file_close()`는 "파일 삭제"가 아니다.
- "이 핸들을 반납"하는 것이다.

---

## 왜 원본 `file`을 그대로 `aux`에 넣으면 위험한가

예를 들어 `load_segment()`에서 이렇게 했다고 생각해 보자.

```c
aux->file = file;
```

겉보기에는 문제 없어 보인다.
지금 당장은 `file`이 살아 있기 때문이다.

하지만 lazy loading은 "지금 당장"이 아니라 "나중"에 읽는다.

### 문제 상황

1. `load()`가 실행 중이다.
2. `load_segment()`가 여러 페이지의 `aux`를 만든다.
3. 각 `aux`가 모두 같은 `file *`를 저장한다.
4. `load()`가 끝난 뒤 원래 `file`이 닫히거나 다른 경로에서 정리된다.
5. 이후 어떤 주소에서 페이지 폴트가 발생한다.
6. `lazy_load_segment()`가 `aux->file`로 읽으려 한다.

이때 `aux->file`은 이미 닫힌 파일 객체를 가리킬 수 있다.

즉:
- 포인터 값은 남아 있어 보여도
- 그 포인터가 가리키는 실제 객체는 이미 무효일 수 있다.

이런 포인터를 dangling pointer라고 생각하면 된다.

그 상태에서

```c
file_read_at(aux->file, ...)
```

를 하면:
- 잘못된 메모리 접근
- 알 수 없는 값 참조
- 운이 나쁘면 커널 패닉

같은 문제가 생길 수 있다.

---

## 왜 `file_reopen(file)`가 해결책인가

각 페이지가 자기 전용 파일 핸들을 하나 따로 가지게 만들면 된다.

즉 `load_segment()`에서 페이지별 `aux`를 만들 때:

```c
aux->file = file_reopen(file);
```

로 넣어준다.

이렇게 하면:

- 원본 `file`은 원본대로 살아간다.
- 각 `aux->file`은 그 페이지만을 위한 독립 핸들이다.
- 원본 `file`이 먼저 닫혀도, `aux->file`은 여전히 유효할 수 있다.

즉 "원본 파일 포인터를 공유"하는 것이 아니라
"각 페이지가 자기 생명주기를 가진 파일 참조를 소유"하게 되는 것이다.

---

## 왜 페이지마다 다시 `reopen`해야 하는가

이 질문도 많이 헷갈린다.

왜 그냥 한 번만 `reopen`해서 여러 `aux`가 같이 쓰면 안 될까?

이유는 소유권과 수명 때문이다.

각 페이지의 `aux`는:
- 서로 다른 시점에 페이지 폴트가 날 수 있고
- 서로 다른 시점에 로드될 수 있고
- 성공/실패 여부도 독립적일 수 있다.

즉 각 `aux`는 자기 자원을 스스로 소유하고 정리할 수 있어야 한다.

만약 여러 페이지가 같은 `aux->file`을 공유하면:

- 먼저 로딩된 페이지가 `file_close(aux->file)`를 해버릴 수 있고
- 아직 로딩되지 않은 다른 페이지들은 죽은 핸들을 보게 된다.

그래서 페이지마다:

- `file_reopen()`
- 자기 전용 `aux`
- 자기 전용 `file_close()`

가 더 안전하다.

---

## 왜 `lazy_load_segment()`에서 `file_close()`를 해야 하는가

`file_reopen(file)`를 했다는 건
"새 파일 핸들 하나를 추가로 생성했다"는 뜻이다.

새로 만든 자원은 언젠가 반드시 반납해야 한다.

`lazy_load_segment()`는 해당 페이지의 파일 내용을 실제로 읽는 시점이다.
읽기가 끝나면 그 페이지는 더 이상 `aux->file`을 쓸 이유가 없다.

따라서 이 시점이 가장 자연스러운 정리 지점이다.

보통 성공 시:

```c
file_close(temp_aux->file);
free(temp_aux);
return true;
```

실패 시에도 마찬가지로:

```c
file_close(temp_aux->file);
free(temp_aux);
return false;
```

처럼 정리해야 누수가 없다.

---

## 실패 경로에서도 `file_close()`가 필요한 이유

이건 자주 놓치는 부분이다.

예를 들어:

```c
if (bytes_read != (off_t) temp_aux->read_bytes) {
    free(temp_aux);
    return false;
}
```

이렇게만 쓰면 `temp_aux->file`은 닫히지 않는다.

즉 `file_reopen()`으로 만든 파일 핸들이 남아 버린다.
이건 자원 누수다.

그래서 실패하더라도:

- `file_close(temp_aux->file);`
- `free(temp_aux);`

를 둘 다 해야 한다.

---

## 왜 `file_close()`와 `free()`의 순서가 중요한가

아래 순서는 안전하다.

```c
file_close(temp_aux->file);
free(temp_aux);
```

하지만 아래 순서는 위험하다.

```c
free(temp_aux);
file_close(temp_aux->file);
```

이유는 간단하다.

`temp_aux->file`에 접근하려면 `temp_aux`가 아직 살아 있어야 한다.

`free(temp_aux)`를 먼저 하면:
- `temp_aux` 구조체 메모리는 이미 해제된다.
- 그 다음 `temp_aux->file`을 읽는 것은 해제된 메모리를 다시 읽는 행위다.

이건 use-after-free 문제다.

즉 자원 정리 순서는 보통 이렇게 생각하면 된다.

1. 구조체 안의 내부 자원 먼저 정리
2. 마지막에 구조체 자체를 `free`

---

## 코드 레벨에서 기억할 핵심 체크포인트

### `load_segment()`에서 해야 할 일

- 페이지마다 `aux`를 새로 만든다.
- `aux->file = file_reopen(file);`
- `aux->ofs = ofs;`
- `aux->read_bytes = page_read_bytes;`
- `aux->zero_bytes = page_zero_bytes;`
- 다음 페이지를 위해 `ofs += page_read_bytes;`

---

### `lazy_load_segment()`에서 해야 할 일

- `file_read_at()`로 `read_bytes`만큼 읽는다.
- 실패하면 `file_close()`와 `free()` 후 `false`
- 성공하면 남은 `zero_bytes`를 `memset()`으로 0 채운다.
- 끝나면 `file_close()`와 `free()` 후 `true`

---

## 자주 하는 오해

### 오해 1. "지금 file이 살아 있으니까 그냥 써도 되지 않나?"

아니다.
lazy loading은 미래 시점의 접근을 위해 예약하는 구조라서
"지금 살아 있음"만으로는 부족하다.

필요한 건:
"페이지 폴트가 실제로 날 때까지도 유효한 참조"다.

---

### 오해 2. "`file_reopen()`은 파일 복사본을 만드는 건가?"

완전히 새로운 파일 내용을 만드는 게 아니다.

같은 실제 파일을 가리키는
"새 열린 파일 객체"를 하나 더 만드는 것이다.

즉 파일 데이터 복사가 아니라 핸들 복제에 가깝다.

---

### 오해 3. "`file_close()`를 하면 파일 자체가 사라지나?"

아니다.
그 핸들을 닫는 것이다.

다른 참조가 남아 있으면 실제 파일 객체나 inode는 계속 살아 있을 수 있다.

---

## 한 줄 요약

`load_segment()`의 원본 `file`은 "현재 로딩 과정용"이고,
`file_reopen()`으로 만든 `aux->file`은 "미래 lazy loading 시점까지 버틸 독립 참조"다.

그래서:

- 페이지마다 `file_reopen()`이 필요하고
- 로딩이 끝나면 `lazy_load_segment()`에서 `file_close()`가 필요하며
- `file_close()`는 `free(aux)`보다 먼저 호출해야 한다.
