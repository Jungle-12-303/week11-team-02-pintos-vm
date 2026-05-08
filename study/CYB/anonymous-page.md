# Anonymous Page 번역

## Anonymous Page

이 프로젝트의 이 부분에서는 anonymous page라고 불리는 non-disk based image를 구현하게 된다.

anonymous mapping은 backing file이나 장치를 가지지 않는다. file-backed page와 달리 이름이 있는 파일 원천(named file source)을 가지지 않기 때문에 anonymous라고 부른다. anonymous page는 실행 파일의 stack과 heap 같은 곳에 사용된다.

`include/vm/anon.h`에는 anonymous page를 설명하기 위한 구조체 `anon_page`가 있다. 현재는 비어 있지만, 구현을 진행하면서 anonymous page에 필요한 정보나 상태를 저장하기 위해 멤버를 추가할 수 있다. 또한 페이지의 일반적인 정보를 담고 있는 `include/vm/page.h`의 `struct page`도 함께 보라. 참고로 anonymous page의 경우에는 그 page 구조체 안에 `struct anon_page anon`이 포함된다.

## Page Initialization with Lazy Loading

lazy loading은 메모리 로딩을 실제로 필요해지는 시점까지 미루는 설계이다. 페이지는 할당되어 있어서, 즉 그에 대응하는 `page struct`는 존재하지만, 전용 물리 프레임은 없고 페이지의 실제 내용도 아직 로드되지 않은 상태이다. 그 내용은 정말 필요해지는 시점에만 로드되는데, 그 시점은 페이지 폴트로 표시된다.

페이지 타입이 세 가지이므로, 각 페이지에 대한 초기화 루틴도 서로 다르다. 아래 절들에서 다시 설명하겠지만, 여기서는 페이지 초기화 흐름의 상위 수준 개요를 제공한다. 먼저 커널이 새 페이지 요청을 받으면 `vm_alloc_page_with_initializer`가 호출된다. initializer는 새 페이지 구조체를 할당하고, 페이지 타입에 맞는 적절한 initializer를 설정한 뒤, 제어를 사용자 프로그램으로 되돌린다. 사용자 프로그램이 실행되다가, 어느 시점에서 자신이 가지고 있다고 믿고 있는 페이지에 접근하려 하지만 그 페이지는 아직 내용을 가지지 않기 때문에 페이지 폴트가 발생한다. fault 처리 과정에서 `uninit_initialize`가 호출되고, 앞서 설정해 둔 initializer를 호출한다. anonymous page에 대해서는 그 initializer가 `anon_initializer`가 되고, file-backed page에 대해서는 `file_backed_initializer`가 된다.

페이지는 `initialize->(page_fault->lazy-load->swap-in>swap-out->...)->destroy`와 같은 생명주기를 가질 수 있다. 이 생명주기의 각 전이에서 요구되는 절차는 페이지 타입(또는 `VM_TYPE`)에 따라 달라지며, 앞 문단은 그중 초기화에 대한 예시였다. 이 프로젝트에서 여러분은 각 페이지 타입에 대해 이러한 전이 과정을 구현하게 된다.

## Lazy Loading for Executable

lazy loading에서는 프로세스가 실행을 시작할 때, 즉시 필요한 메모리 부분만 메인 메모리에 로드된다. 이는 바이너리 이미지를 한 번에 모두 메모리로 올리는 eager loading과 비교했을 때 오버헤드를 줄일 수 있다.

lazy loading을 지원하기 위해, 우리는 `include/vm/vm.h`에 `VM_UNINIT`라는 페이지 타입을 도입한다. 모든 페이지는 처음에 `VM_UNINIT` 페이지로 생성된다. 또한 초기화되지 않은 페이지를 위한 페이지 구조체인 `struct uninit_page`를 `include/vm/uninit.h`에 제공한다. 초기화되지 않은 페이지를 생성, 초기화, 파괴하는 함수들은 `include/vm/uninit.c`에서 찾을 수 있다. 여러분은 이 함수들을 나중에 완성해야 한다.

페이지 폴트가 발생하면, page fault handler(`userprog/exception.c`의 `page_fault`)는 제어를 `vm/vm.c`의 `vm_try_handle_fault`로 넘기고, 그 함수는 먼저 그것이 유효한 페이지 폴트인지 검사한다. 여기서 유효하다는 것은 invalid한 접근을 하는 fault가 아니라는 뜻이다. bogus fault라면, 어떤 내용을 페이지에 로드하고 사용자 프로그램으로 제어를 되돌린다.

bogus page fault에는 세 가지 경우가 있다. lazy-loaded page, swapped-out page, 그리고 write-protected page(See Copy-on-Write (Extra))이다. 지금은 첫 번째 경우인 lazy-loaded page만 생각하라. 이것이 lazy loading을 위한 페이지 폴트라면, 커널은 `vm_alloc_page_with_initializer`에서 이전에 설정해 둔 initializer 중 하나를 호출하여 세그먼트를 lazy load한다. 여러분은 `userprog/process.c`의 `lazy_load_segment`를 구현해야 한다.

`vm_alloc_page_with_initializer()`를 구현하라. 전달된 `vm_type`에 따라 적절한 initializer를 가져오고, 그것을 사용해 `uninit_new`를 호출해야 한다.

```c
bool vm_alloc_page_with_initializer (enum vm_type type, void *va,
        bool writable, vm_initializer *init, void *aux);
```

주어진 타입의 초기화되지 않은 페이지를 생성하라. uninit page의 `swap_in` handler는 타입에 따라 자동으로 페이지를 초기화하고, 주어진 `AUX`와 함께 `INIT`를 호출한다. page struct를 얻은 뒤에는, 그 페이지를 프로세스의 supplementary page table에 삽입하라. `vm.h`에 정의된 `VM_TYPE` 매크로가 유용할 수 있다.

page fault handler는 호출 체인을 따라가다가, 결국 `swap_in`을 호출할 때 `uninit_initialize`에 도달한다. 우리는 그 함수에 대한 완전한 구현을 제공한다. 다만 여러분의 설계에 따라 `uninit_initialize`를 수정해야 할 수도 있다.

```c
static bool uninit_initialize (struct page *page, void *kva);
```

첫 번째 fault 시 페이지를 초기화한다. 템플릿 코드는 먼저 `vm_initializer`와 `aux`를 가져온 뒤, 함수 포인터를 통해 대응되는 `page_initializer`를 호출한다. 여러분의 설계에 따라 이 함수를 수정해야 할 수도 있다.

필요에 따라 `vm/anon.c`의 `vm_anon_init`과 `anon_initializer`를 수정할 수 있다.

```c
void vm_anon_init (void);
```

anonymous page subsystem을 초기화한다. 이 함수 안에서 anonymous page와 관련된 모든 설정을 할 수 있다.

```c
bool anon_initializer (struct page *page,enum vm_type type, void *kva);
```

이 함수는 먼저 `page->operations` 안에 anonymous page를 위한 handler들을 설정한다. 현재 비어 있는 구조체인 `anon_page` 안의 몇몇 정보를 갱신해야 할 수도 있다. 이 함수는 anonymous page(`VM_ANON`)를 위한 initializer로 사용된다.

`userprog/process.c`에 `load_segment`와 `lazy_load_segment`를 구현하라. 실행 파일로부터의 세그먼트 로딩을 구현해야 한다. 이 페이지들은 모두 lazy하게 로드되어야 하며, 즉 커널이 그들에 대한 페이지 폴트를 가로챌 때에만 로드되어야 한다.

여러분은 프로그램 로더의 핵심인 `userprog/process.c`의 `load_segment` 안 루프를 수정해야 한다. 루프를 한 번 돌 때마다, 이 코드는 pending page object를 만들기 위해 `vm_alloc_page_with_initializer`를 호출한다. 페이지 폴트가 발생했을 때가 바로 세그먼트가 실제로 파일로부터 로드되는 시점이다.

```c
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
        uint32_t read_bytes, uint32_t zero_bytes, bool writable);
```

현재 코드는 메인 루프 안에서 파일에서 읽어야 할 바이트 수와 0으로 채워야 할 바이트 수를 계산한다. 그리고 pending object를 만들기 위해 `vm_alloc_page_with_initializer`를 호출한다. 여러분은 `vm_alloc_page_with_initializer`에 전달할 `aux` 인자로 사용할 보조 값들을 설정해야 한다. 바이너리 로딩에 필요한 정보를 담는 구조체를 만드는 것이 좋을 수 있다.

```c
static bool lazy_load_segment (struct page *page, void *aux);
```

여러분은 `load_segment`에서 `vm_alloc_page_with_initializer`의 네 번째 인자로 `lazy_load_segment`가 전달되는 것을 눈치챘을 수 있다. 이 함수는 실행 파일 페이지를 위한 initializer이며, 페이지 폴트가 발생하는 시점에 호출된다. 이 함수는 `page struct`와 `aux`를 인자로 받는다. `aux`는 여러분이 `load_segment`에서 설정한 정보이다. 이 정보를 사용해 세그먼트를 읽어올 파일을 찾아야 하고, 결국 그 세그먼트를 메모리로 읽어와야 한다.

`userprog/process.c`의 `setup_stack`도 새로운 메모리 관리 시스템에 맞게 조정해야 한다. 첫 번째 stack page는 lazy하게 할당할 필요가 없다. 이 페이지는 load 시점에 명령줄 인자와 함께 바로 할당하고 초기화할 수 있으며, fault가 발생할 때까지 기다릴 필요가 없다. stack을 식별할 수 있는 방법을 제공해야 할 수도 있다. `vm/vm.h`의 `vm_type` 안에 있는 auxiliary marker들(예: `VM_MARKER_0`)을 사용해 그 페이지를 표시할 수 있다.

마지막으로, `vm_try_handle_fault` 함수를 수정하여, fault가 난 주소에 대응하는 page struct를 supplemental page table에 조회함으로써 찾도록 하라. 이를 위해 `spt_find_page`를 사용하면 된다.

모든 요구사항을 구현하고 나면, project 2의 테스트들 중 `fork`를 제외한 모든 테스트가 통과해야 한다.

## Supplemental Page Table - Revisit

이제 copy와 clean up 연산을 지원하기 위해 supplemental page table 인터페이스를 다시 살펴본다. 이러한 연산은 프로세스를 생성할 때(좀 더 정확히는 child process를 만들 때)와 프로세스를 파괴할 때 필요하다. 자세한 내용은 아래에서 설명한다. 이 시점에서 supplemental page table을 다시 다루는 이유는, 위에서 구현한 초기화 함수들 중 일부를 사용하고 싶을 수 있기 때문이다.

`vm/vm.c`에 `supplemental_page_table_copy`와 `supplemental_page_table_kill`을 구현하라.

```c
bool supplemental_page_table_copy (struct supplemental_page_table *dst,
    struct supplemental_page_table *src);
```

supplemental page table을 `src`에서 `dst`로 복사한다. 이는 child가 parent의 실행 문맥을 상속해야 할 때(즉 `fork()`) 사용된다. `src`의 supplemental page table에 있는 각 페이지를 순회하며, 그 엔트리를 `dst`의 supplemental page table에 정확히 복사하라. 여러분은 uninit page를 할당하고, 그것들을 즉시 claim해야 한다.

```c
void supplemental_page_table_kill (struct supplemental_page_table *spt);
```

supplemental page table이 가지고 있던 모든 자원을 해제한다. 이 함수는 프로세스가 종료될 때(`userprog/process.c`의 `process_exit()`) 호출된다. 테이블 안의 페이지 엔트리들을 순회하면서 각 페이지에 대해 `destroy(page)`를 호출해야 한다. 이 함수 안에서는 실제 페이지 테이블(`pml4`)과 물리 메모리(`palloc`으로 할당된 메모리)에 대해 신경 쓸 필요가 없다. caller가 supplemental page table 정리가 끝난 뒤 그것들을 정리한다.

## Page Cleanup

`vm/uninit.c`의 `uninit_destroy`와 `vm/anon.c`의 `anon_destroy`를 구현하라. 이것들은 초기화되지 않은 페이지에 대한 destroy 연산의 handler이다. 초기화되지 않은 페이지들은 결국 다른 페이지 객체들로 변환되지만, 프로세스가 종료할 때 여전히 uninit page가 남아 있을 수 있다.

```c
static void uninit_destroy (struct page *page);
```

page struct가 가지고 있던 자원을 해제한다. 페이지의 `vm type`을 확인하고 그에 맞게 처리하는 것이 좋을 수 있다.

지금은 anonymous page만 처리하면 된다. file-backed page 정리를 위해서는 나중에 이 함수를 다시 다루게 된다.

```c
static void anon_destroy (struct page *page);
```

anonymous page가 가지고 있던 자원을 해제한다. page struct 자체를 명시적으로 해제할 필요는 없다. caller가 그것을 해제해야 한다.

이제 project 2의 모든 테스트가 통과해야 한다.
