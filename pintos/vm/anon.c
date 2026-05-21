/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "lib/kernel/bitmap.h"
#include "threads/vaddr.h"
#include "lib/string.h"

/* DO NOT MODIFY BELOW LINE */
struct disk *swap_disk;
struct bitmap* swap_bitmap;
struct lock swap_lock;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);
static bool anon_copy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.copy = anon_copy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
// 디스크를 가져와서, 디스크에 얼마나 저장할 수 있는지 페이지 단위로 계산해서 배열을 만들어줌!
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1,1);

	ASSERT(swap_disk != NULL);

	// 단위 계산용 분자에 섹터 단위가 있다면, 분모에 섹터 단위를 나눠서 단위를 없애버리자..
	// 디스크 사이즈 = 섹터
	// 512 * size 
	swap_bitmap = bitmap_create(disk_size(swap_disk) / (PGSIZE / DISK_SECTOR_SIZE)); // 4096(PGSIZE) / 512(DISK_SECTOR_SIZE) = 8

	lock_init(&swap_lock);

}

/* Initialize the file mapping */
// 함수 시그니처를 다른 initializer들과 맞추기 위해서 여기서는 kva를 사용하지 않지만, 매개변수로 받음.
// 이 페이지를 anon 페이지로 취급하기 시작하게 만들기 위해서 사용하는 함수
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	if(page == NULL){
		return false;
	}

	page->operations = &anon_ops;	
	page->anon.type = type;
	page->anon.swap_status = false;
	// 이 페이지는 스왑 슬롯을 보유 X == 아직 swap disk에 내려간 적 X
	page->anon.swap_slot = -1; // -1 : “유효한 슬롯 없음”을 나타내는 특별한 값

	return true;
}

/* Swap in the page by read contents from the swap disk. */
// 디스크 -> 메모리
// TODO: [5]
static bool
anon_swap_in (struct page *page, void *kva) {

	if (page == NULL || kva == NULL){
		return false;
	}
	
	struct anon_page *anon_page = &page->anon;
	size_t sector_per_page = (PGSIZE / DISK_SECTOR_SIZE); // 하나의 페이지가 가지는 섹터의 수

	lock_acquire(&swap_lock);

	if(anon_page->swap_status){

		if(swap_bitmap == NULL || swap_disk == NULL || anon_page->swap_slot == -1){
			lock_release(&swap_lock);
			return false;
		}

		//1. page크기만큼의 정보가 필요하니까 그 만큼 반복해줘야 함.
		for(int i = 0; i < sector_per_page; i++){
			
			// 디스크의 내용을 kva에 넣음.
			
			// 디스크 내에서 읽어야하는 범위는 
			// 시작: page의 slot * 하나의 페이지가 가지는 섹터 수 
			// 끝: page의 slot * 하나의 페이지가 가지는 섹터 수 + 하나의 페이지가 가지는 섹터 수 - 1 
			disk_read(swap_disk, anon_page->swap_slot * sector_per_page + i, (char*)kva + DISK_SECTOR_SIZE * i);
			
		}
		bitmap_set(swap_bitmap, anon_page->swap_slot, 0);
		
		anon_page->swap_status = false;
		anon_page->swap_slot = -1;
	}else {
		// 어디에, 어떤 값으로, 얼만큼 채울 것인가
		memset(kva, 0, PGSIZE);
	}

	lock_release(&swap_lock);

	return true;
}

/* Swap out the page by writing contents to the swap disk. */
// TODO: [6]
static bool
anon_swap_out (struct page *page) {
	if (page == NULL || page->frame == NULL || page->frame->kva == NULL){
		return false;
	}

	lock_acquire(&swap_lock);

	if(swap_bitmap == NULL || swap_disk == NULL || page->anon.swap_status == true){
		lock_release(&swap_lock);
		return false;
	}

	struct anon_page *anon_page = &page->anon;
	size_t sector_per_page = (PGSIZE / DISK_SECTOR_SIZE); // 하나의 페이지가 가지는 섹터의 수

	// 디스크에서 비어있는 위치 찾기
	size_t bitmap_idx = bitmap_scan(swap_bitmap, 0, 1, 0);
	if(bitmap_idx == BITMAP_ERROR){
		lock_release(&swap_lock);
		return false;
	}

	for(int i = 0; i < sector_per_page; i++){
		
		// 메모리의 내용을 디스크에 넣어줌
		disk_write(swap_disk, bitmap_idx * sector_per_page + i, (char*)page->frame->kva + DISK_SECTOR_SIZE * i);
		
	}
	bitmap_set(swap_bitmap, bitmap_idx, 1);
	
	anon_page->swap_status = true;
	anon_page->swap_slot = bitmap_idx;
	
	lock_release(&swap_lock);

	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
// TODO: [7]
static void
anon_destroy (struct page *page) {

	if (page == NULL){
		return;
	}
	// 2가지 상황
	lock_acquire(&swap_lock);

	struct anon_page *anon_page = &page->anon;
	if(anon_page->swap_status){

		if(swap_bitmap == NULL || anon_page->swap_slot == -1){
			lock_release(&swap_lock);
			return;
		}

		bitmap_set(swap_bitmap, anon_page->swap_slot, 0);
		
		anon_page->swap_status = false;
		anon_page->swap_slot = -1;
	}

	lock_release(&swap_lock);

}

static bool
anon_copy (struct page *page) {
	struct supplemental_page_table *dst = &thread_current()->spt;

	if (!vm_alloc_page(page_get_type(page),page->va, page->writable)){
		return false;
	}

	struct page *temp_page = spt_find_page(dst, page->va);
	
	
	if (temp_page == NULL) {
		return false;
	}
	
	if(!vm_claim_page(temp_page->va)){
		spt_remove_page(dst, temp_page);
		return false;
	}
	
	if(temp_page->frame == NULL || temp_page->frame->kva == NULL){
		spt_remove_page(dst, temp_page);
		return false;
	}

	if(page->anon.swap_status == 0){ // 부모 페이지가 스왑되지 않아서 바로 카피할 수 있는 상태라면
		
		if(page->frame == NULL || page->frame->kva == NULL) {
			spt_remove_page(dst, temp_page);
			return false;
		}

		memcpy(temp_page->frame->kva, page->frame->kva, PGSIZE);
		return true;

	} else { // 부모 페이지가 스왑된 상태라면
			if (swap_bitmap == NULL || swap_disk == NULL || page->anon.swap_slot == (size_t) -1) {
				spt_remove_page(dst, temp_page);
				return false;
		}

		size_t sector_per_page = PGSIZE / DISK_SECTOR_SIZE;

		lock_acquire(&swap_lock);
		for (size_t i = 0; i < sector_per_page; i++) {
			disk_read(swap_disk,
					page->anon.swap_slot * sector_per_page + i,
					(char *)temp_page->frame->kva + DISK_SECTOR_SIZE * i);
		}
		lock_release(&swap_lock);
		return true;
	}
	
}