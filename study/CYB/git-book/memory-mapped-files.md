# Memory Mapped Files 번역

## Memory Mapped Files

이 절에서는 memory-mapped page를 구현한다. anonymous page와 달리 memory-mapped page는 file-backed mapping이다. 페이지의 내용은 어떤 기존 파일의 내용과 대응된다. 페이지 폴트가 발생하면 물리 프레임을 즉시 할당하고, 파일의 내용을 메모리로 복사한다. memory-mapped page가 unmapped되거나 swap out될 때, 그 내용에 가해진 변경 사항은 파일에 반영된다.

## `mmap` and `munmap` System Call

`mmap`과 `munmap`은 memory-mapped file을 위한 두 개의 시스템 콜이다. VM 시스템은 `mmap` 영역의 페이지를 lazy loading 방식으로 로드해야 하며, mapping 자체에 사용된 파일을 backing store로 사용해야 한다. 이 두 시스템 콜을 구현하기 위해 `vm/file.c`에 정의된 `do_mmap`과 `do_munmap`을 구현하고 사용해야 한다.

```c
void *mmap (void *addr, size_t length, int writable, int fd, off_t offset);
```

`fd`로 열린 파일의 `offset` 바이트부터 시작하는 `length` 바이트를, 프로세스의 가상 주소 공간에서 `addr`에 매핑한다. 파일 전체는 `addr`에서 시작하는 연속된 가상 페이지들에 매핑된다. 파일 길이가 `PGSIZE`의 배수가 아니라면, 마지막 매핑 페이지의 일부 바이트는 파일 끝을 넘어 "튀어나오게" 된다. 이 바이트들은 페이지 폴트 시 0으로 채우고, 페이지를 디스크에 다시 쓸 때는 버려야 한다. 성공하면 파일이 매핑된 가상 주소를 반환한다. 실패하면 파일을 매핑할 수 있는 유효한 주소가 아닌 `NULL`을 반환해야 한다.

다음 경우 `mmap`은 실패할 수 있다.

- `fd`로 열린 파일의 길이가 0바이트인 경우
- `addr`가 페이지 정렬(page-aligned)되어 있지 않은 경우
- 매핑하려는 페이지 범위가 기존의 어떤 매핑과라도 겹치는 경우
  실행 파일 로드 시 매핑된 페이지와 스택도 포함된다.
- `addr`가 `0`인 경우
  Linux에서는 `addr == NULL`일 때 커널이 적절한 주소를 찾지만, 여기서는 단순화를 위해 주어진 `addr`에만 매핑을 시도한다. 따라서 Pintos의 일부 코드가 가상 페이지 0이 매핑되지 않았다고 가정하므로 `addr == 0`은 반드시 실패해야 한다.
- `length`가 0인 경우
- 표준 입력과 표준 출력을 나타내는 파일 디스크립터인 경우

memory-mapped page 역시 anonymous page처럼 lazy하게 할당해야 한다. 페이지 객체를 만들기 위해 `vm_alloc_page_with_initializer` 또는 `vm_alloc_page`를 사용할 수 있다.

```c
void munmap (void *addr);
```

지정한 주소 범위 `addr`에 대한 매핑을 해제한다. 이 `addr`은 같은 프로세스가 이전에 `mmap`으로 반환받았고 아직 해제하지 않은 시작 주소여야 한다.

모든 매핑은 프로세스가 종료할 때, `exit`이든 다른 어떤 종료 방식이든 상관없이 암묵적으로 해제된다. 매핑이 명시적으로든 암묵적으로든 해제될 때, 프로세스가 수정한 모든 페이지는 파일에 다시 기록되어야 하고, 수정되지 않은 페이지는 기록되어서는 안 된다. 이후 해당 페이지들은 프로세스의 virtual page 목록에서 제거된다.

파일을 닫거나 삭제해도 해당 파일에 대한 매핑은 해제되지 않는다. 한번 생성된 매핑은 Unix 관례에 따라 `munmap`이 호출되거나 프로세스가 종료될 때까지 유효하다. 자세한 내용은 Removing an Open File을 참고하라. 각 매핑마다 독립적인 파일 참조를 얻기 위해 `file_reopen`을 사용해야 한다.

둘 이상의 프로세스가 같은 파일을 매핑하더라도, 서로 일관된 데이터를 보아야 한다는 요구는 없다. Unix는 이를 같은 물리 페이지를 공유하게 해서 처리하며, `mmap` 시스템 콜은 매핑을 shared로 할지 private, 즉 copy-on-write로 할지 지정하는 인자도 가진다.

설계에 따라 `vm/vm.c`의 `vm_file_init`과 `file_backed_initializer`를 수정하고 싶을 수 있다.

```c
void vm_file_init (void);
```

file-backed page subsystem을 초기화한다. 이 함수 안에서 file-backed page와 관련된 설정을 할 수 있다.

```c
bool file_backed_initializer (struct page *page, enum vm_type type, void *kva);
```

file-backed page를 초기화한다. 이 함수는 먼저 `page->operations`에 file-backed page를 위한 handler들을 설정한다. 또한 page 구조체 안의 file-backed page 관련 정보, 예를 들어 어떤 파일이 이 페이지의 backing file인지 같은 정보를 갱신해야 할 수 있다.

```c
static void file_backed_destroy (struct page *page);
```

연관된 파일을 닫으면서 file-backed page를 파괴한다. 내용이 dirty 상태라면 변경 사항이 파일에 반영되도록 반드시 write back 해야 한다. 이 함수 안에서 page 구조체 자체를 해제할 필요는 없다. `file_backed_destroy`를 호출한 쪽에서 그것을 처리해야 한다.
