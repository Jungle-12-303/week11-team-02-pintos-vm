# Pintos VM Test Dependency 정리

## 기준
- 대상 테스트 목록: `pintos/tests/vm/Make.tests`
- 현재 구현 상태 확인 파일:
  - `pintos/vm/vm.c`
  - `pintos/vm/anon.c`
  - `pintos/vm/file.c`
  - `pintos/userprog/process.c`
  - `pintos/userprog/syscall.c`
- 현재 코드 기준으로 VM 핵심 로직은 거의 스켈레톤 상태다.
  - `supplemental_page_table`
  - page fault 처리
  - frame 할당/eviction
  - swap in/out
  - lazy load
  - `mmap` / `munmap`
  - fork 시 SPT 복제

## 먼저 깔아야 하는 공통 기반
VM 테스트들은 서로 완전히 독립적이지 않다. 아래 기반이 먼저 있어야 뒤 테스트가 의미 있게 돈다.

1. `Project 2` 사용자 프로그램 기반
- `fork`, `exec`, `wait`
- 파일 syscall (`open`, `read`, `write`, `close`, `remove`, `seek`)
- 사용자 포인터 검증

2. VM 공통 기반
- `supplemental_page_table` 자료구조
- `spt_find_page`, `spt_insert_page`, `spt_remove_page`
- `vm_alloc_page_with_initializer`
- `vm_claim_page`
- `vm_try_handle_fault`

3. Lazy loading 기반
- `lazy_load_segment`
- `uninit` -> `anon` / `file` 전이
- 실행 파일 세그먼트 lazy load

4. Stack growth 기반
- stack page 표시
- fault 주소가 stack 확장 대상인지 판별
- 첫 stack page 즉시 claim

5. Frame / eviction / swap 기반
- frame table
- victim 선정 정책
- anon page swap out / swap in
- dirty bit / accessed bit 활용

6. File-backed page 기반
- file-backed page 메타데이터
- `mmap`, `munmap`
- dirty page write-back
- overlap / alignment / zero-length / kernel-space validation

7. Fork + VM 복제 기반
- `supplemental_page_table_copy`
- lazy 상태 복제
- claimed anon page 복제
- file-backed mapping 상속 정책 결정
  - 이 과제 테스트 기준으로 `mmap` 영역은 자식에게 상속되면 안 됨

## 추천 구현 순서
1. SPT + page claim + lazy executable segment
2. stack setup + stack growth + 잘못된 접근 종료
3. anonymous page 동작 검증 (`page-linear`, `page-shuffle`)
4. fork와 결합되는 paging 계열 (`page-parallel`, `page-merge-*`)
5. anon swap
6. file-backed page + `mmap` / `munmap`
7. `mmap` robustness
8. mixed workload (`lazy-file`, `swap-file`, `swap-iter`, `swap-fork`)

## 큰 흐름 의존성
- `pt-grow-stack` 계열이 되면 stack fault 처리 골격이 잡힌다.
- `page-linear` / `page-shuffle` 이 되면 anon page 기본 동작과 lazy executable segment가 안정화된다.
- `page-parallel` / `page-merge-*` 는 `fork` + VM 복제까지 요구한다.
- `lazy-anon` 은 "미리 다 올리지 않고 fault 시점에 page가 올라온다"를 직접 검증한다.
- `mmap-read` 가 file-backed page의 시작점이다.
- `mmap-write`, `mmap-clean`, `mmap-exit`, `mmap-remove`, `mmap-close` 는 dirty/write-back/lifetime 관리까지 요구한다.
- `swap-anon` 이후에 `swap-file`, `swap-iter`, `swap-fork` 로 가는 흐름이 자연스럽다.

## 테스트별 의존성 / 난이도
난이도는 "이 테스트를 통과시키기 위해 필요한 커널 구현량" 기준의 체감치다.

| 테스트 | 주로 보는 것 | 직접 선행 구현 | 난이도 |
|---|---|---|---:|
| `pt-grow-stack` | 1 page stack growth | page fault, stack 판별, stack claim | 4/10 |
| `pt-grow-stk-sc` | syscall 중 stack growth | `pt-grow-stack`, syscall buffer fault 처리 | 6/10 |
| `pt-big-stk-obj` | 큰 stack growth | `pt-grow-stack`, 다중 page 연속 확장 | 5/10 |
| `pt-bad-addr` | 잘못된 주소 접근 종료 | page fault 유효성 검사 | 2/10 |
| `pt-bad-read` | 잘못된 버퍼로 read 시 종료 | 포인터 검증 또는 fault 처리 일관성 | 3/10 |
| `pt-write-code` | code segment write 금지 | read-only page 권한 유지 | 3/10 |
| `pt-write-code2` | syscall로 code write 금지 | `pt-write-code`, syscall buffer validation | 4/10 |
| `pt-grow-bad` | 너무 먼 stack fault 거부 | stack growth heuristic | 4/10 |
| `page-linear` | 대량 anon page read/write | lazy executable segment, anon claim | 5/10 |
| `page-parallel` | 여러 프로세스 동시 paging | `page-linear`, `fork/exec/wait`, SPT copy | 7/10 |
| `page-merge-seq` | 순차 subprocess + large memory | `page-linear`, 파일 I/O 안정성, fork 후 주소공간 유지 | 6/10 |
| `page-merge-par` | 병렬 subprocess + large memory | `page-merge-seq`, `page-parallel`, 자원 경쟁 안정성 | 7/10 |
| `page-merge-stk` | 큰 stack 사용하는 child와 paging | `page-merge-par`, stack growth 안정화 | 8/10 |
| `page-merge-mm` | child 내부 `mmap` 사용 | `page-merge-par`, `mmap-read`, `mmap-write` | 9/10 |
| `page-shuffle` | 반복 read/write 무결성 | `page-linear`, 다수 page 접근 안정성 | 5/10 |
| `mmap-read` | file-backed page 읽기 | `mmap` 기본 검증, file-backed page lazy load | 6/10 |
| `mmap-close` | 파일 닫아도 mapping 유지 | `mmap-read`, mapping이 file object 수명과 분리 | 7/10 |
| `mmap-unmap` | `munmap` 후 접근 불가 | `mmap-read`, page unmap / SPT 정리 | 7/10 |
| `mmap-overlap` | 겹치는 mapping 금지 | `mmap-read`, SPT overlap 탐지 | 5/10 |
| `mmap-twice` | 같은 파일의 다중 mapping | `mmap-read`, mapping별 독립 추적 | 6/10 |
| `mmap-write` | mapping을 통한 write-back | `mmap-read`, dirty page write-back | 7/10 |
| `mmap-ro` | read-only mapping 보호 | `mmap-read`, write-protect fault 처리 | 6/10 |
| `mmap-exit` | `munmap` 없이 종료해도 write-back | `mmap-write`, process exit 시 mapping 정리 | 8/10 |
| `mmap-shuffle` | 큰 file-backed mapping 반복 수정 | `mmap-write`, 다중 dirty page 처리 | 8/10 |
| `mmap-bad-fd` | invalid fd 거부 | `mmap` 인자 검증 | 2/10 |
| `mmap-clean` | dirty 아닌 page는 write-back 금지 | `mmap-write`, dirty bit 판별 | 8/10 |
| `mmap-inherit` | `mmap` 영역 비상속 | `fork`, SPT copy 정책 분기 | 8/10 |
| `mmap-misalign` | 비정렬 주소 거부 | `mmap` 인자 검증 | 2/10 |
| `mmap-null` | `NULL` mapping 거부 | `mmap` 인자 검증 | 1/10 |
| `mmap-over-code` | code 위 mapping 금지 | `mmap-read`, 기존 SPT와 충돌 탐지 | 4/10 |
| `mmap-over-data` | data 위 mapping 금지 | `mmap-read`, 기존 SPT와 충돌 탐지 | 4/10 |
| `mmap-over-stk` | stack 위 mapping 금지 | `mmap-read`, stack page 충돌 탐지 | 5/10 |
| `mmap-remove` | 삭제된 파일도 mapping 유지 | `mmap-close`, inode/file lifetime 이해 | 8/10 |
| `mmap-zero` | zero-length file 접근 안전성 | `mmap` 예외 케이스 처리 | 3/10 |
| `mmap-bad-fd2` | `stdin` mmap 거부 | `mmap` 인자 검증 | 1/10 |
| `mmap-bad-fd3` | `stdout` mmap 거부 | `mmap` 인자 검증 | 1/10 |
| `mmap-zero-len` | length 0 거부 | `mmap` 인자 검증 | 1/10 |
| `mmap-off` | non-zero aligned offset mapping | `mmap-read`, offset별 file load, write-back offset 유지 | 7/10 |
| `mmap-bad-off` | misaligned offset 거부 | `mmap` 인자 검증 | 2/10 |
| `mmap-kernel` | kernel address mapping 거부 | `mmap` 주소 범위 검증 | 3/10 |
| `lazy-file` | file-backed lazy load 직접 검증 | `mmap-read`, fault 시 1 page만 load | 7/10 |
| `lazy-anon` | anon lazy load 직접 검증 | lazy executable segment, unclaimed page 유지 | 6/10 |
| `swap-file` | file-backed page 재적재 안정성 | `mmap-read`, eviction, file-backed reload | 8/10 |
| `swap-anon` | anon swap in/out | frame eviction, swap disk bitmap | 8/10 |
| `swap-iter` | anon + file-backed 혼합 swap | `swap-anon`, `swap-file`, victim 정책 안정성 | 9/10 |
| `swap-fork` | 다수 child + swap pressure | `swap-anon`, `fork`, SPT copy, 자원 회수 | 9/10 |

## 구현할 때 묶어서 보면 좋은 세트

### 1. Fault / stack 세트
- `pt-grow-stack`
- `pt-grow-stk-sc`
- `pt-big-stk-obj`
- `pt-grow-bad`
- `pt-bad-addr`
- `pt-bad-read`
- `pt-write-code`
- `pt-write-code2`

핵심:
- `vm_try_handle_fault`
- stack growth 조건
- writable / not_present 구분
- user/kernel address 검증

### 2. Anonymous paging 세트
- `page-linear`
- `page-shuffle`
- `lazy-anon`

핵심:
- lazy executable segment
- anonymous page claim
- page별 실제 fault 시점 로드

### 3. Fork 결합 세트
- `page-parallel`
- `page-merge-seq`
- `page-merge-par`
- `page-merge-stk`

핵심:
- `supplemental_page_table_copy`
- 부모/자식의 page 상태 분리
- stack growth와 fork 공존

### 4. `mmap` 기본 세트
- `mmap-read`
- `mmap-twice`
- `mmap-unmap`
- `mmap-close`
- `mmap-write`
- `mmap-ro`

핵심:
- file-backed page initializer
- page 단위 file read
- unmap 시 page table / SPT cleanup
- writable 플래그 반영

### 5. `mmap` robustness 세트
- `mmap-bad-fd*`
- `mmap-null`
- `mmap-zero-len`
- `mmap-misalign`
- `mmap-over-*`
- `mmap-overlap`
- `mmap-bad-off`
- `mmap-kernel`

핵심:
- `do_mmap()` 초입 validation
- 기존 매핑/세그먼트와 충돌 체크

### 6. `mmap` lifetime / write-back 세트
- `mmap-clean`
- `mmap-exit`
- `mmap-remove`
- `mmap-inherit`
- `mmap-off`
- `mmap-shuffle`
- `page-merge-mm`

핵심:
- dirty page만 write-back
- process exit 시 `munmap` equivalent 수행
- file close/remove 이후에도 mapping 유지
- offset-aware mapping
- fork 시 `mmap` 비상속

### 7. Swap 세트
- `swap-anon`
- `swap-file`
- `swap-iter`
- `swap-fork`

핵심:
- frame table
- victim 선택
- swap slot 관리
- anon/file-backed page의 eviction 차이
- process 종료 시 자원 회수

## 실전 우선순위 한 줄 추천
가장 안전한 순서는 `pt-* -> page-linear/page-shuffle/lazy-anon -> page-parallel/page-merge-* -> mmap-read 계열 -> mmap robustness -> mmap write-back 계열 -> swap-*` 이다.
