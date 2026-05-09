# Memory Management 번역

## Memory Management

가상 메모리 시스템을 지원하기 위해서는 가상 페이지와 물리 프레임을 효과적으로 관리해야 한다. 이는 어떤 (가상 또는 물리) 메모리 영역이 사용 중인지, 어떤 목적을 위해 사용되는지, 누가 사용하고 있는지 등을 추적해야 한다는 뜻이다. 여러분은 먼저 supplemental page table을 다루고, 그 다음 물리 프레임을 다루게 된다. 이해를 돕기 위해, 여기서는 가상 페이지를 "page", 물리 페이지를 "frame"이라고 부른다.

## Page Structure and Operations

### struct page

`include/vm/vm.h`에 정의된 page는 가상 메모리의 한 페이지를 나타내는 구조체이다. 이 구조체는 해당 페이지에 대해 우리가 알아야 하는 모든 필요한 데이터를 저장한다. 현재 템플릿에서 이 구조체는 다음과 같은 형태를 가진다.

```c
struct page {
  const struct page_operations *operations;
  void *va;              /* @origin Address in terms of user space */
  struct frame *frame;   /* @origin Back reference for frame */

  union {
    struct uninit_page uninit;
    struct anon_page anon;
    struct file_page file;
#ifdef EFILESYS
    struct page_cache page_cache;
#endif
  };
};
```

주석 번역:

- `@origin Address in terms of user space`
  사용자 공간 기준의 가상 주소
- `@origin Back reference for frame`
  이 페이지가 올라간 프레임을 다시 가리키는 역참조 포인터

이 구조체는 page operations(아래에서 설명), 가상 주소, 물리 프레임을 가진다. 즉 `struct page`는 "가상 페이지 하나의 공통 메타데이터"를 담는 상위 구조체라고 보면 된다.

각 필드를 이해할 때는 다음처럼 읽으면 된다.

- `operations`
  이 페이지 타입에 맞는 함수 테이블이다. `swap_in`, `swap_out`, `destroy` 같은 동작을 어떤 함수로 처리할지 결정한다.
- `va`
  이 페이지가 담당하는 사용자 가상 주소다. 보통 페이지 시작 주소 단위로 관리한다.
- `frame`
  현재 이 페이지가 어떤 물리 프레임에 올라가 있는지 가리킨다. 아직 메모리에 올라오지 않았다면 `NULL`일 수 있다.

추가로 `union` 필드를 가지고 있다. union은 하나의 메모리 영역을 여러 타입이 "공유해서" 쓰는 특별한 자료형이다. union 안에는 여러 멤버가 있지만, 한 시점에 실제 의미를 가지는 멤버는 하나뿐이다. 이는 우리 시스템의 한 페이지가 `uninit_page`, `anon_page`, `file_page`, 또는 `page_cache` 중 하나가 될 수 있다는 뜻이다.

예를 들어 어떤 페이지가 anonymous page라면(See Anonymous Page), `page` 구조체는 멤버 중 하나로 `struct anon_page anon` 필드를 갖게 된다. 이때 실제로 의미 있게 사용하는 union 멤버는 `anon`이고, `anon_page`는 anonymous page에 대해 우리가 유지해야 하는 모든 필요한 정보를 담는다.

여기서 union은 "인터페이스처럼" 쓰인다고 이해해도 된다. 정확히 말하면 union 자체가 인터페이스는 아니지만, `struct page`가 공통 인터페이스 역할을 하고, union은 "페이지 타입별 상세 구현 데이터"를 담는 자리로 동작한다.

즉 구조를 나눠 보면 다음과 같다.

- 공통 인터페이스
  `struct page`
- 공통 동작 선택
  `operations`
- 타입별 상세 상태
  `union { uninit, anon, file, ... }`

객체지향적으로 비유하면:

- `struct page`는 부모 타입
- `operations`는 가상 함수 테이블
- `union` 안의 각 구조체는 자식 타입별 추가 필드

그래서 코드에서는 공통적으로 `struct page *page` 하나만 들고 다니면서도,

- 이 페이지가 아직 초기화 전인지
- anonymous page인지
- file-backed page인지

에 따라 서로 다른 데이터와 서로 다른 함수를 사용할 수 있다.

### Page Operations

위에서 설명했듯이, 그리고 `include/vm/vm.h`에 정의되어 있듯이, 페이지는 `VM_UNINIT`, `VM_ANON`, 또는 `VM_FILE`이 될 수 있다. 페이지에 대해서는 swapping in, swapping out, destroying page 같은 여러 동작이 필요하다. 각 페이지 타입마다 이러한 동작을 수행하기 위해 필요한 단계와 작업이 다르다. 다시 말해, `VM_ANON` 페이지와 `VM_FILE` 페이지에는 서로 다른 destroy 함수가 호출되어야 한다. 한 가지 방법은 각 함수에서 switch-case 문법을 사용해 각 경우를 처리하는 것이다. 우리는 이를 다루기 위해 객체지향 프로그래밍의 "class inheritance" 개념을 도입한다. 물론 C 언어에는 실제 "class"도 없고 "inheritance"도 없지만, Linux 같은 실제 운영체제 코드에서 사용하는 방식과 유사하게 함수 포인터를 이용해 이 개념을 구현한다.

함수 포인터는 지금까지 배운 다른 포인터들과 마찬가지로 포인터이지만, 메모리 안의 함수, 즉 실행 가능한 코드를 가리킨다. 함수 포인터는 실행 시점의 값에 따라 별도의 검사 없이 특정 함수를 호출해 실행할 수 있는 간단한 방법을 제공하므로 유용하다. 우리의 경우 코드 수준에서는 단순히 `destroy(page)`를 호출하는 것만으로 충분하고, 컴파일러는 적절한 함수 포인터를 호출함으로써 페이지 타입에 따라 알맞은 destroy 루틴을 선택하게 된다.

페이지 연산을 위한 구조체 `struct page_operations`는 `include/vm/vm.h`에 정의되어 있다. 이 구조체는 3개의 함수 포인터를 담고 있는 함수 테이블로 생각하면 된다.

```c
struct page_operations {
  bool (*swap_in) (struct page *, void *);
  bool (*swap_out) (struct page *);
  void (*destroy) (struct page *);
  enum vm_type type;
};
```

이제 page_operation 구조체를 어디에서 찾을 수 있는지 보자. `include/vm/vm.h`의 `struct page`를 보면 `operations`라는 필드가 있다. 이제 `vm/file.c`로 가 보면 함수 프로토타입들보다 앞에 `page_operations` 구조체인 `file_ops`가 선언되어 있다. 이것이 file-backed page를 위한 함수 포인터 테이블이다. `.destroy` 필드는 `file_backed_destroy` 값을 가지며, 이 함수는 페이지를 파괴하는 함수로 같은 파일 안에 정의되어 있다.

함수 포인터 인터페이스를 통해 `file_backed_destroy`가 어떻게 호출되는지 이해해 보자. 예를 들어 `vm/vm.c`의 `vm_dealloc_page(page)`가 호출되었고, 이 페이지가 우연히 file-backed page(`VM_FILE`)라고 하자. 함수 내부에서는 `destroy(page)`를 호출한다. `destroy(page)`는 `include/vm/vm.h`에서 다음과 같은 매크로로 정의되어 있다.

```c
#define destroy(page) if ((page)->operations->destroy) (page)->operations->destroy (page)
```

이는 destroy 함수를 호출하는 것이 실제로는 `(page)->operations->destroy(page)`를 호출한다는 뜻이며, 이 destroy 함수는 page 구조체에서 가져온 것이다. 페이지가 `VM_FILE` 페이지이기 때문에, 그 `.destroy` 필드는 `file_backed_destroy`를 가리킨다. 그 결과 file-backed page를 위한 destroy 루틴이 수행된다.

## Implement Supplemental Page Table

이 시점에서 여러분의 Pintos는 메모리의 가상-물리 매핑을 관리하기 위한 페이지 테이블(`pml4`)을 가지고 있다. 그러나 이것만으로는 충분하지 않다. 이전 절에서 논의했듯이, 페이지 폴트 처리와 자원 관리를 위해 각 페이지에 대한 추가 정보를 담는 supplementary page table도 필요하다. 따라서 프로젝트 3의 첫 번째 작업으로서 supplemental page table의 기본 기능들을 구현할 것을 권장한다.

`vm/vm.c`에 supplemental page table 관리 함수들을 구현하라.

먼저 여러분의 Pintos에서 supplemental page table을 어떻게 설계할지 결정해야 한다. 자신의 supplemental page table을 설계한 뒤, 그 설계에 맞게 아래 세 함수를 구현하라.

```c
void supplemental_page_table_init (struct supplemental_page_table *spt);
```

Supplemental page table을 초기화한다. 어떤 자료구조를 사용할지는 여러분이 선택할 수 있다. 이 함수는 새로운 프로세스가 시작할 때(`userprog/process.c`의 `initd`)와, 프로세스가 fork될 때(`userprog/process.c`의 `__do_fork`) 호출된다.

```c
struct page *spt_find_page (struct supplemental_page_table *spt, void *va);
```

주어진 supplemental page table에서 `va`에 대응하는 `struct page`를 찾는다. 실패하면 `NULL`을 반환한다.

```c
bool spt_insert_page (struct supplemental_page_table *spt, struct page *page);
```

주어진 supplemental page table에 `struct page`를 삽입한다. 이 함수는 해당 가상 주소가 주어진 supplemental page table에 이미 존재하지 않는지 확인해야 한다.

## Frame Management

이제부터 모든 페이지는 메모리가 생성될 때의 메타데이터만 들고 있는 것이 아니다. 따라서 물리 메모리를 관리하기 위한 다른 방식이 필요하다. `include/vm/vm.h`에는 물리 메모리를 나타내는 `struct frame`이 존재한다. 현재 이 구조체는 다음과 같은 모습이다.

```c
/* The representation of "frame" */
struct frame {
  void *kva;
  struct page *page;
};
```

이 구조체는 두 개의 필드만 가진다. `kva`는 커널 가상 주소이고, `page`는 page 구조체이다. frame management 인터페이스를 구현하면서 더 많은 멤버를 추가해도 된다.

`vm/vm.c`에 `vm_get_frame`, `vm_claim_page`, `vm_do_claim_page`를 구현하라.

```c
static struct frame *vm_get_frame (void);
```

`palloc_get_page`를 호출해 user pool에서 새로운 물리 페이지를 얻는다. user pool에서 페이지를 성공적으로 얻었다면, frame도 할당하고, 그 멤버들을 초기화한 뒤 반환해야 한다. `vm_get_frame`을 구현한 뒤에는, 모든 user space page(`PALLOC_USER`)가 이 함수를 통해 할당되어야 한다. 지금은 페이지 할당 실패 시 swap out을 처리할 필요는 없다. 그런 경우는 우선 `PANIC("todo")`로 표시해 두면 된다.

```c
bool vm_do_claim_page (struct page *page);
```

페이지를 claim한다는 것은, 즉 해당 페이지에 대해 물리 프레임을 할당한다는 뜻이다. 먼저 `vm_get_frame`을 호출해 프레임을 얻는다(이 부분은 템플릿에서 이미 되어 있다). 그런 다음 MMU를 설정해야 한다. 다시 말해, 페이지 테이블 안에 가상 주소에서 물리 주소로의 매핑을 추가해야 한다. 반환값은 이 연산이 성공했는지 여부를 나타내야 한다.

```c
bool vm_claim_page (void *va);
```

`va`에 대한 페이지를 claim한다. 먼저 페이지를 얻어야 하고, 그 다음 그 페이지를 인자로 하여 `vm_do_claim_page`를 호출한다.
