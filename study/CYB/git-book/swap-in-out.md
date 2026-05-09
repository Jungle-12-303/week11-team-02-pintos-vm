# Swap In/Out 번역

## Swap In/Out

메모리 스와핑은 물리 메모리 사용률을 최대화하기 위한 메모리 회수 기법이다. 메인 메모리의 프레임이 모두 할당되면, 시스템은 사용자 프로그램의 추가 메모리 할당 요청을 더 이상 처리할 수 없다. 한 가지 해결책은 현재 사용되지 않는 메모리 프레임을 디스크로 swap out하는 것이다. 이렇게 하면 일부 메모리 자원이 해제되어 다른 애플리케이션이 사용할 수 있게 된다.

스와핑은 운영체제가 수행한다. 시스템이 메모리가 부족하다고 판단한 상태에서 메모리 할당 요청을 받으면, swap disk로 내보낼 페이지를 하나 선택한다. 그리고 그 메모리 프레임의 정확한 상태를 디스크에 복사한다. 이후 프로세스가 swap out된 페이지에 접근하려고 하면, 운영체제는 그 정확한 내용을 다시 메모리로 가져와 페이지를 복구한다.

축출 대상으로 선택되는 페이지는 anonymous page일 수도 있고 file-backed page일 수도 있다. 이 절에서는 각 경우를 처리한다.

모든 스와핑 연산은 명시적으로 직접 호출되지 않고 함수 포인터를 통해 호출된다. 이 함수 포인터들은 `struct page_operations file_ops`의 멤버이며, 각 페이지 initializer가 자신의 연산으로 등록하게 된다.

## Anonymous Page

`vm/anon.c`의 `vm_anon_init`과 `anon_initializer`를 수정하라. anonymous page는 이를 위한 backing storage를 따로 가지지 않는다. anonymous page의 스와핑을 지원하기 위해, temporal backing storage인 swap disk가 제공된다. 여러분은 anonymous page의 스와핑을 구현하기 위해 이 swap disk를 사용해야 한다.

```c
void vm_anon_init (void);
```

이 함수에서는 swap disk를 설정해야 한다. 또한 swap disk의 사용 중인 영역과 비어 있는 영역을 관리할 자료구조도 필요하다. swap 영역 역시 `PGSIZE`(4096바이트) 단위로 관리해야 한다.

```c
bool anon_initializer (struct page *page, enum vm_type type, void *kva);
```

이 함수는 anonymous page를 위한 initializer이다. 스와핑을 지원하기 위해 `anon_page`에 필요한 정보를 추가해야 한다.

이제 `vm/anon.c`에서 `anon_swap_in`과 `anon_swap_out`을 구현하여 anonymous page의 스와핑을 지원하라. 어떤 페이지든 swap in되기 전에 먼저 swap out되어 있어야 하므로, 일반적으로 `anon_swap_out`을 먼저 구현하는 편이 자연스럽다. 데이터 내용을 swap disk로 옮기고, 다시 안전하게 메모리로 가져와야 한다.

```c
static bool anon_swap_in (struct page *page, void *kva);
```

swap disk에 저장된 데이터 내용을 읽어 메모리로 복원함으로써 anonymous page를 swap in한다. 데이터가 저장된 위치는 페이지가 swap out될 때 page 구조체에 저장되어 있어야 한다. swap table도 함께 갱신하는 것을 잊지 마라(Managing the Swap Table 참고).

```c
static bool anon_swap_out (struct page *page);
```

메모리의 내용을 디스크로 복사하여 anonymous page를 swap out한다. 먼저 swap table을 사용해 디스크에서 빈 swap slot을 찾고, 그 슬롯에 페이지 전체 데이터를 복사한다. 데이터가 저장된 위치는 page 구조체에 기록해야 한다. 더 이상 사용할 수 있는 빈 slot이 없다면, 커널을 panic시켜도 된다.

## File-Mapped Page

file-backed page의 내용은 파일에서 오므로, mmap된 파일 자체가 backing store로 사용되어야 한다. 즉 file-backed page를 축출할 때는, 그 페이지를 매핑했던 파일에 내용을 다시 기록해야 한다. `vm/file.c`에서 `file_backed_swap_in`과 `file_backed_swap_out`을 구현하라. 설계에 따라 `file_backed_init`과 `file_initializer`를 수정해도 된다.

```c
static bool file_backed_swap_in (struct page *page, void *kva);
```

파일의 내용을 읽어 `kva`에 적재함으로써 페이지를 swap in한다. 이 과정에서는 파일 시스템과 동기화가 필요하다.

```c
static bool file_backed_swap_out (struct page *page);
```

페이지의 내용을 파일에 다시 기록하여 swap out한다. 먼저 해당 페이지가 dirty한지 확인하는 편이 좋다. dirty하지 않다면 파일 내용을 수정할 필요가 없다. swap out을 마친 뒤에는 그 페이지의 dirty bit를 반드시 꺼야 한다.
