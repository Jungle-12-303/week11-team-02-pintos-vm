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
#include <string.h>
#include "vm/file.h"
#include "userprog/process.h"
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
vm_stack_growth (void *addr UNUSED) {
	struct thread *current = thread_current ();
	struct supplemental_page_table *spt;
	char *current_stack_bottom;
	char *fault_page;

	RETURN_IF(current == NULL || current->stack_bottom == NULL);

	spt = &current->spt;
	current_stack_bottom = current->stack_bottom;
	fault_page = pg_round_down (addr);

	while (fault_page < current_stack_bottom) {
		struct page *stack_page;

		current_stack_bottom -= PGSIZE;

		RETURN_IF(!vm_alloc_page (VM_ANON | VM_MARKER_0, current_stack_bottom, true));

		if (!vm_claim_page (current_stack_bottom)) {
			stack_page = spt_find_page (spt, current_stack_bottom);
			if (stack_page != NULL)
				spt_remove_page (spt, stack_page);
			return;
		}

		current->stack_bottom = current_stack_bottom;
	}
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
			
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* True: access by user, false: access by kernel. */

	// not_present == true는 “페이지가 현재 메모리에 없어서 fault”
	// not_present == false는 "페이지는 존재하는데, 접근 
	RETURN_VALUE_IF(!is_user_vaddr(addr) || addr == NULL, false);

	// 프레임 있음
	if (!not_present) {
		return false;
	}
	// 프레임 없음
	else {
		page = spt_find_page(spt, addr);
		if (page == NULL) {
			// 현재 폴트 주소 영역이 유저니? 커널이니?
			// 커널이면 스레드의 유저 스택을 사용해
			uintptr_t *user_rsp = user ? f->rsp : thread_current()->user_rsp;
			
			// 현재 폴트 주소가 유저 스택 영역이 맞니?
			if( (addr < USER_STACK) && // 폴트 주소가 스택 시작 주소에서 아래인가?
				(addr >= (USER_STACK - ONE_MB)) && // 폴트 주소가 스택 끝 주소에서 위인가?
				(addr >= (char*)user_rsp - 8) ) { // 폴트 주소가 스택 주소 근처인가?

				vm_stack_growth(addr);
				return true;
			}
			return false;
		}
		if( (write && !page->writable)){ 
			return false;
		}
		return vm_do_claim_page (page);
	}
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
// 부모가 가지고 있던 가상 메모리 구조를 자식도 똑같이 가지게 만든다
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
			//src는 부모 spt, dst는 자식 spt
	struct hash_iterator i;
	//i를 src의 hash_table에 대한 걸로 설정
	hash_first(&i, &src->hash_table);
	
	// 비어있지 않은 bucket 찾아서 거기의 hash_elem 주고, 그 bucket의 hash_elem 있으면 계속 넘어간다.
	while(hash_next(&i))
	{
		// found_elem을 통해서 page가 되고 싶음.
		struct page* page = hash_entry(i.elem, struct page, hash_elem);

		
		switch(VM_TYPE(page->operations->type))
		{
			case VM_UNINIT:

				//temp_aux 메모리 할당
				struct segment_load_aux *parent_aux = page->uninit.aux;
				struct segment_load_aux *temp_aux = malloc(sizeof(*temp_aux));
				if(temp_aux == NULL)
				{
					return false;
				}
				//temp_aux에 원본 aux 복사
				memcpy(temp_aux, parent_aux, sizeof(*temp_aux));

				// 같은 파일을 가리키는 자식전용 file 핸들을 새로 여는 것
				// 현재 스레드가 자식 스레드여서 사용 가능
				// 부모 스레드와는 다른 파일 객체 사용
				temp_aux->file = file_reopen(parent_aux->file);
				if(temp_aux->file == NULL)
				{
					free(temp_aux);
					return false;
				}
				// 현재 스레드가 자식 스레드여서 사용 가능
				if(!vm_alloc_page_with_initializer(page->uninit.type, page->va, page->writable, page->uninit.init, temp_aux))
				{
					return false;
				}
				
				//UNINIT page여서 매핑은 안한다
				
				break;
			case VM_ANON:
				if(!vm_alloc_page(VM_ANON, page->va, page->writable))
				{
					return false;
				}
				

				//부모 스레드 frame 있는지 확인
				if(page->frame == NULL)
				{
					return false;
				}
				
				//자식 스레드의 spt에 child_page를 가진 페이지가 있으면 물리 메모리에 매핑
				if(!vm_claim_page(page->va))
				{
					return false;
				}

				//자식 스레드 spt에서 child_page 찾는다
				struct page* child_page = spt_find_page(dst, page->va);

				if(child_page == NULL)
				{
					return false;
				}

				// 부모 frame 내용 -> 자식 frame 내용 복사
				memcpy(child_page->frame->kva, page->frame->kva, PGSIZE);
				break;

			case VM_FILE:
				// struct file_page *file_page = &page->file;

				// struct file_info* child_aux = malloc(sizeof *child_aux);

				vm_alloc_page(VM_FILE, page->va, page->writable);

				struct page* child_page = spt_find_page(dst, page->va);

				// 부모 스레드 페이지의 파일 정보를 자식 스레드 페이지에 저장
				child_page->file.ofs = page->file.ofs;
				child_page->file.read_bytes = page->file.read_bytes;
				child_page->file.zero_bytes = page->file.zero_bytes;
				child_page->file.file = file_reopen(page->file.file);

				// 부모 페이지 frame 있나 확인
				if(page->frame == NULL)
				{
					return false;
				}

				// 자식 페이지 페이지 테이블에 매핑
				if(!vm_claim_page(page->va))
				{
					return false;
				}
				
				// 자식 페이지 찾는다
				struct page* child_page = spt_find_page(dst, page->va);
				if(child_page == NULL)
				{
					return false;
				}

				memcpy(child_page->frame->kva, page->frame->kva, PGSIZE);
				break;
		}

		
		
	}

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
