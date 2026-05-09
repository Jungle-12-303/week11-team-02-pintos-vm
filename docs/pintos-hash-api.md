# Pintos Hash API 정리

Pintos의 `hash`는 자료구조 안에 `struct hash_elem`을 멤버로 넣어두고, 그
`hash_elem`을 기준으로 삽입/탐색/삭제하는 체이닝 해시 테이블이다.

SPT 구현에서는 보통 `struct page`를 가상 주소 `va` 기준으로 저장할 때 쓴다.

## 기본 구조

`struct page` 안에 `hash_elem`을 추가한다.

```c
struct page {
	const struct page_operations *operations;
	void *va;
	struct frame *frame;

	struct hash_elem hash_elem;

	union {
		struct uninit_page uninit;
		struct anon_page anon;
		struct file_page file;
	};
};
```

SPT에는 `struct hash`를 둔다.

```c
struct supplemental_page_table {
	struct hash pages;
};
```

`vm.h`에서 hash 타입을 쓰려면 include가 필요하다.

```c
#include "lib/kernel/hash.h"
```

## hash_entry

`hash` API는 `struct hash_elem *`만 돌려준다. 실제 바깥 구조체로 되돌릴 때
`hash_entry()`를 쓴다.

```c
struct page *page = hash_entry (elem, struct page, hash_elem);
```

의미는 다음과 같다.

```text
hash_elem 주소 -> 그 hash_elem을 멤버로 가진 struct page 주소
```

## 콜백 함수

`hash_init()`을 하려면 hash 함수와 less 함수를 넘겨야 한다.

### Hash Function

```c
static uint64_t
page_hash (const struct hash_elem *e, void *aux UNUSED) {
	const struct page *p = hash_entry (e, struct page, hash_elem);
	return hash_bytes (&p->va, sizeof p->va);
}
```

`va` 값을 key로 쓰기 때문에 `p->va`를 해싱한다.

### Less Function

```c
static bool
page_less (const struct hash_elem *a,
           const struct hash_elem *b,
           void *aux UNUSED) {
	const struct page *pa = hash_entry (a, struct page, hash_elem);
	const struct page *pb = hash_entry (b, struct page, hash_elem);
	return pa->va < pb->va;
}
```

Pintos hash는 두 원소가 같은지 직접 묻지 않고, less 함수로 판단한다.

```text
a == b 조건:
!less(a, b) && !less(b, a)
```

## 초기화

```c
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	hash_init (&spt->pages, page_hash, page_less, NULL);
}
```

## 삽입

```c
bool
spt_insert_page (struct supplemental_page_table *spt, struct page *page) {
	struct hash_elem *old;

	old = hash_insert (&spt->pages, &page->hash_elem);
	return old == NULL;
}
```

`hash_insert()` 반환값:

```text
NULL      삽입 성공
non-NULL  같은 key의 기존 원소가 있어서 삽입 실패
```

## 탐색

찾을 때는 임시 `struct page`를 만들어 key 역할을 할 `va`만 채워도 된다.

```c
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	struct page temp;
	struct hash_elem *e;

	temp.va = pg_round_down (va);
	e = hash_find (&spt->pages, &temp.hash_elem);

	if (e == NULL)
		return NULL;

	return hash_entry (e, struct page, hash_elem);
}
```

VM에서는 fault address가 페이지 중간 주소일 수 있으므로 보통
`pg_round_down(va)`를 key로 사용한다.

## 삭제

```c
void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	hash_delete (&spt->pages, &page->hash_elem);
	vm_dealloc_page (page);
}
```

`hash_delete()`는 해시 테이블에서 빼기만 한다. 실제 메모리 해제는 호출자가
해야 한다.

## 전체 해제

```c
static void
page_destroy (struct hash_elem *e, void *aux UNUSED) {
	struct page *page = hash_entry (e, struct page, hash_elem);
	vm_dealloc_page (page);
}

void
supplemental_page_table_kill (struct supplemental_page_table *spt) {
	hash_destroy (&spt->pages, page_destroy);
}
```

`hash_destroy()`에 destructor를 넘기면 각 원소마다 destructor를 호출한 뒤
bucket 배열을 해제한다.

## 자주 쓰는 API

| API | 역할 |
| --- | --- |
| `hash_init` | 해시 테이블 초기화 |
| `hash_insert` | 중복이 없으면 삽입 |
| `hash_replace` | 중복이 있으면 기존 원소를 교체 |
| `hash_find` | 같은 key의 원소 탐색 |
| `hash_delete` | 같은 key의 원소 제거 |
| `hash_destroy` | 전체 원소 정리 후 해시 테이블 해제 |
| `hash_apply` | 모든 원소에 함수 적용 |
| `hash_entry` | `hash_elem *`에서 바깥 구조체 포인터 얻기 |
| `hash_bytes` | 바이트 배열 해싱 |
| `hash_string` | 문자열 해싱 |
| `hash_int` | 정수 해싱 |

## SPT에서 필요한 최소 세트

SPT 구현에서는 보통 이 정도만 쓰면 충분하다.

```text
hash_init
hash_insert
hash_find
hash_delete
hash_destroy
hash_entry
hash_bytes
```

## 주의점

- `hash_elem`은 반드시 저장할 구조체 안에 멤버로 들어가야 한다.
- 같은 원소를 동시에 두 해시 테이블에 넣을 수 없다.
- `hash_delete()`는 메모리를 free하지 않는다.
- `hash_destroy()`는 destructor를 넘겼을 때만 각 원소를 정리한다.
- 순회 중에 삽입/삭제하면 iterator가 깨질 수 있다.
