/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/mmu.h"
#include "threads/init.h"

#define ONE_MB (1 << 20) // 1MB

static struct list frame_table;
static struct lock frame_table_lock;

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();

	list_init (&frame_table);
	lock_init (&frame_table_lock);

#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT);

	struct supplemental_page_table *spt = &thread_current ()->spt;
	
	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) != NULL) {
		return false;
	}

	/* Create the page, fetch the initialier according to the VM type,
	* and then create "uninit" page struct by calling uninit_new. You
	* should modify the field after calling the uninit_new. */

	struct page *page = malloc(sizeof(struct page));
	
	if(page == NULL){
		return false;
	}
	
	/* 함수 자체를 담는 변수
		“struct page *, enum vm_type, void *를 인자로 받고,
		bool을 반환하는 함수”를 가리키는 포인터
		=> 페이지 종류에 따라 초기화 방식이 다르기 때문에 필요함
		*/
	bool (*initializer) (struct page *, enum vm_type, void *kva);

	// `type` 값에는 순수한 페이지 타입만 들어있는 게 아니라 
	// 다른 플래그 비트가 함께 들어있어서 `VM_TYPE(type)`로 하위 비트만 추출하여 기본 타입만 확인
	if(VM_TYPE(type) == VM_ANON){
		initializer = anon_initializer; // 이름만 쓰면 함수의 주소를 저장
	}else if(VM_TYPE(type) == VM_FILE){
		initializer = file_backed_initializer; // 괄호까지 붙여버리면 당장 호출
	}else{
		free(page);
		return false;
	}

	// page를 아직 실제 내용은 없지만,  나중에 초기화할 방법이 등록된 pending page 상태로 설정
	uninit_new(page, pg_round_down(upage), init, type, aux, initializer);
	page->writable = writable;


	/* Insert the page into the spt. */
	if(!spt_insert_page(spt, page)){
		free(page);
		return false;
	}
	return true;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	struct page *page = NULL;
	struct page tmp_page; // 가짜 페이지 하나 생성

	tmp_page.va = pg_round_down(va); // 가짜 페이지에 가상주소 넣기

	// 주어진 spt에서 가상주소와 일치하는 페이지의 해시 elem을 찾음
	struct hash_elem* found_elem = hash_find(&spt->hash_table, &tmp_page.hash_elem);

	if(found_elem == NULL){ // 가상주소와 일치하는 해시elem을 못 찾음
		return NULL;
	}else{
		// found_elem을 통해서 page가 되고 싶음.
		page = hash_entry(found_elem, struct page, hash_elem);
		return page;	
	}

}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt,
		struct page *page ) {
	int succ = false;
	/* TODO: Fill this function. */
	if(spt == NULL || page == NULL)
	{
		return succ;
	}

	struct hash_elem* hash_elem = hash_insert(&spt->hash_table, &page->hash_elem);
	
	if(hash_elem == NULL)
	{
		succ = true;
	}
	return succ;
}

bool
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	hash_delete (&spt->hash_table, &page->hash_elem);
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = malloc (sizeof *frame);
	if (frame == NULL) {
		return NULL;
	}

	frame->kva = palloc_get_page (PAL_USER);

	//palloc failed, evict.
	if (frame->kva == NULL) {
		free (frame);
		frame = vm_evict_frame ();
		if (frame == NULL) {
			return NULL;
		}
	}
	else {
		//palloc success, initialize the frame struct.
		frame->page = NULL;
		lock_acquire (&frame_table_lock);
		list_push_back (&frame_table, &frame->elem);
		lock_release (&frame_table_lock);
	}

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr,uintptr_t rsp, bool write) {
	void *dst = pg_round_down(addr);
	vm_alloc_page(VM_ANON|VM_MARKER_0, dst ,write);
	vm_claim_page(dst);
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f, void *addr,
		bool user, bool write, bool not_present) {
	struct supplemental_page_table *spt = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	uintptr_t rsp = NULL;

	if(f == NULL || addr == NULL || !not_present){
		return false;
	}
	page = spt_find_page(spt,addr);
	if (page == NULL){
		if (user){
			rsp = f->rsp;
		} else {
			rsp = thread_current()->user_rsp;
		}

		if (!(addr < USER_STACK && rsp - (uintptr_t)addr <= 8 && addr > USER_STACK - ONE_MB)){
			return false;
		}

		vm_stack_growth(addr, rsp, write);

		page = spt_find_page(spt,addr);
		// 실제로 처리 되었는 지
		if(page->frame != NULL){
			return true;
		}
	}
	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
// 할당할 페이지를 요청합니다 va. 
// 먼저 페이지를 가져온 다음, 가져온 페이지를 사용하여 vm_do_claim_page 함수를 호출해야 합니다.
bool
vm_claim_page (void *va) {
	struct page *page = NULL;
	/* TODO: Fill this function */
	struct thread* cur_thread = thread_current();
	page = spt_find_page(&cur_thread->spt, va);

	if(page == NULL)
	{
		return false;
	}

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	if(frame == NULL)
	{
		return false;
	}
	/* Set links */
	frame->page = page;
	page->frame = frame;

	if((!pml4_set_page (thread_current()->pml4, page->va, frame->kva, page->writable)) || (!swap_in(page,frame->kva))){
		lock_acquire (&frame_table_lock);
		list_remove(&frame->elem);
		lock_release (&frame_table_lock);

		page->frame = NULL;
		palloc_free_page(frame->kva);
		free(frame);
		return false;
	}

	return true;
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	hash_init(&spt->hash_table, page_hash, hash_less, &spt->hash_table.aux);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}


// SPT를 구현하기 위한 함수이므로 여기에 위치

/*va를 해시테이블의 키값으로 변환하는 함수*/
/* struct page의 가상 페이지 주소(va)를 해시값으로 바꾼다.
	이 페이지를 어느 버킷에 넣을지 계산하는 규칙
*/
uint64_t page_hash(const struct hash_elem *e, void *aux UNUSED){
	
	/* hash_elem이 들어 있는 실제 struct page를 찾는다. */
	struct page* page = hash_entry(e, struct page, hash_elem);

	/* 페이지 시작 주소값 자체를 바이트 단위로 해시해서 반환한다. */
	return hash_bytes (&page->va, sizeof(page->va));
}

bool hash_less(const struct hash_elem *a, const struct hash_elem *b, void *aux){

	struct page* page_a = hash_entry(a, struct page, hash_elem);
	struct page* page_b = hash_entry(b, struct page, hash_elem);

	return page_a->va < page_b->va;
}
