# VM Fork Call Flow

## 왜 이 함수가 중요한가

`supplemental_page_table_copy()`는 `fork()` 전체 흐름 안에서  
**부모 프로세스의 가상 메모리 상태를 자식 프로세스에게 복제하는 핵심 단계**이다.

이 함수가 성공해야만 자식 프로세스는 부모와 동일한 메모리 의미를 가진 상태로 실행을 시작할 수 있다.

---

## 전체 호출 흐름

```text
User Program
  -> fork() system call
  -> syscall_handler()
  -> process_fork()
  -> thread_create(..., __do_fork, ...)
  -> __do_fork()
  -> supplemental_page_table_copy(&child->spt, &parent->spt)
  -> 성공 시 자식의 pml4 유지 + fd table 복제 + do_iret()
  -> 자식 프로세스 실행 시작
```

---

## 호출 체인에서의 위치

```text
fork()
 └─ process_fork()
    └─ __do_fork()
       ├─ 부모의 intr_frame 복사
       ├─ child pml4 생성
       ├─ process_activate(child)
       ├─ supplemental_page_table_init(&child->spt)
       ├─ supplemental_page_table_copy(&child->spt, &parent->spt)
       ├─ fd_table 복제
       ├─ 자식의 fork 반환값을 0으로 설정
       └─ do_iret()로 유저 모드 진입
```

즉 `supplemental_page_table_copy()`는  
**자식 프로세스가 실제로 유저 모드로 진입하기 직전에 반드시 통과해야 하는 메모리 복제 단계**이다.

---

## 실패 시 의미

`supplemental_page_table_copy()`가 `false`를 반환하면:

1. 자식의 주소 공간 구성이 실패한 것으로 본다
2. `__do_fork()`는 `goto error` 경로로 이동한다
3. 자식은 `fork_success = false`, `exit_status = -1` 상태로 종료된다
4. 부모는 정상적인 자식 프로세스를 얻지 못한다

즉 이 함수의 실패는 단순한 보조 실패가 아니라  
**fork 자체의 실패**를 의미한다.

---

## 성공 시 의미

`supplemental_page_table_copy()`가 `true`를 반환하면:

- 자식 SPT가 부모 SPT의 메모리 의미를 이어받았고
- 자식의 pml4 위에서 해당 가상 메모리 구조를 사용할 준비가 끝났으며
- 이후 문맥 전환(context switch)과 유저 모드 진입이 가능해진다

즉 이 함수는  
**fork의 “메모리 복제 게이트”** 역할을 한다.

---

## 발표용 핵심 문장

> `supplemental_page_table_copy()`는 `fork()` 흐름에서  
> 부모의 가상 메모리 상태를 자식에게 독립적으로 복제하는 단계이며,  
> 이 함수가 성공해야만 자식 프로세스가 올바른 주소 공간을 가진 채 문맥 전환되어 실행을 시작할 수 있다.

---

## Mermaid Graph

```mermaid
flowchart TD
    A[User fork call] --> B[syscall handler]
    B --> C[process_fork]
    C --> D[thread_create child]
    D --> E[enter __do_fork]

    E --> F[copy parent intr_frame]
    F --> G[create child pml4]
    G --> H[activate child]
    H --> I[init child spt]
    I --> J[copy parent spt to child]

    J -->|fail| K[goto error]
    K --> L[mark fork failed]
    L --> M[set exit status minus one]
    M --> N[thread_exit]

    J -->|success| O[duplicate fd table]
    O --> P[set child fork return 0]
    P --> Q[do_iret]
    Q --> R[start child user execution]
```

```mermaid
flowchart LR
    A[부모 SPT src] --> C[supplemental_page_table_copy]
    B[자식 SPT dst] --> C

    C --> D{복사 성공 여부}
    D -->|성공| E[자식 SPT가 부모 메모리 의미를 이어받음]
    E --> F[자식 주소 공간 준비 완료]
    F --> G[문맥 전환 가능]
    G --> H[자식 실행 시작]

    D -->|실패| I[fork 실패]
    I --> J[자식 종료]
```

---

## spt_copy 내부 로직

```mermaid
flowchart TD
    A[supplemental_page_table_copy 시작] --> B[src hash_table 순회]
    B --> C[현재 page 가져오기]
    C --> D{page 타입 확인}

    D -->|VM_UNINIT| E[aux 복사]
    E --> F[file_reopen]
    F --> G[vm_alloc_page_with_initializer 호출]
    G --> H[자식 SPT에 lazy page 등록]

    D -->|VM_ANON| I[자식에 anon page 생성]
    I --> J[spt_find_page로 temp_page 찾기]
    J --> K[vm_do_claim_page temp_page]
    K --> L{부모 anon 상태 확인}

    L -->|resident| M[부모 frame kva를 자식 frame kva로 memcpy]
    L -->|swapped out| N[부모 swap slot에서 디스크 읽기]
    N --> O[자식 frame kva에 페이지 내용 복원]

    D -->|VM_FILE| P[file page 분기 처리 예정]

    H --> Q{다음 page 존재 여부}
    M --> Q
    O --> Q
    P --> Q

    Q -->|yes| C
    Q -->|no| R[true 반환]
```

```mermaid
flowchart LR
    A[부모 page] --> B{page 타입}

    B -->|UNINIT| C[설계도 복사]
    C --> D[aux와 initializer를 자식에게 등록]

    B -->|ANON resident| E[현재 메모리 내용 복사]
    E --> F[부모 frame에서 자식 frame으로 memcpy]

    B -->|ANON swapped out| G[swap 디스크 내용 복사]
    G --> H[부모 swap slot을 읽어 자식 frame에 채움]
```

```mermaid
flowchart LR
    A[현재 page] --> B{page 타입}

    B -->|VM_UNINIT| C[lazy load 정보 복사]
    B -->|VM_ANON| D[실제 페이지 내용 복사]
    B -->|VM_FILE| E[file page 분기 처리]
```

---

## VM_UNINIT 분기 그래프

```mermaid
flowchart TD
    A{page 타입 식별} -->|VM_UNINIT| B[aux 구조체 메모리 확보]
    B --> C[부모 aux 내용을 자식용 aux로 복사]
    C --> D[file_reopen으로 파일 참조 복제]
    D --> E[vm_alloc_page_with_initializer 호출]
    E --> F[자식 SPT에 lazy page 등록]
    F --> G[자식도 나중에 같은 방식으로 페이지 초기화 가능]
```

---

## VM_ANON 분기 그래프

```mermaid
flowchart TD
    A{page 타입 식별} -->|VM_ANON| B[자식에 새 anon page 생성]
    B --> C[spt_find_page로 temp_page 찾기]
    C --> D[vm_do_claim_page로 자식 frame 확보]
    D --> E{부모 anon 상태 확인}

    E -->|resident| F[부모 frame kva를 자식 frame kva로 memcpy]
    E -->|swapped out| G[부모 swap slot에서 디스크 읽기]
    G --> H[자식 frame kva에 페이지 내용 복원]

    F --> I[자식이 부모와 같은 실제 메모리 내용 확보]
    H --> I
```
