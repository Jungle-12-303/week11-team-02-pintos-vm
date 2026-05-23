/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include <string.h>
#include "threads/vaddr.h"
#include "devices/disk.h"

//page 하나가 sector 몇 개짜리인지 계산하는 값
#define SECTORS_PER_PAGE PGSIZE/ DISK_SECTOR_SIZE
#define INVALID_SWAP_SLOT ((uint32_t) -1)

/* DO NOT MODIFY BELOW LINE */
//ram에서 쫓겨난 anom_page 내용 저장하는 디스크
static struct disk *swap_disk;

//swap_disk에서 몇번 slot이 비어있는지 보려고 생성
static struct bitmap* swap_table;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = NULL;
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;

	anon_page->swap_slot = INVALID_SWAP_SLOT;
	return true;
}

/* Swap in the page by read contents from the swap disk. */
//swap disk에 쫓겨났던 anonymous page 내용을 다시 물리 frame으로 복구하는 함수
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;

	//페이지의 swap slot이 유효하다면
	if(anon_page->swap_slot != INVALID_SWAP_SLOT)
	{	
		// swap_slot
		size_t slot = anon_page->swap_slot;

		// 디스크에서의 sector 시작 위치
		size_t start_sector = slot * SECTORS_PER_PAGE;

		//
		for(int i =0; i < SECTORS_PER_PAGE; i++)
		{
			// 디스크의 특정 섹터에서 512바이트 읽어서 메모리 버퍼에 넣는다
			disk_read(swap_disk, start_sector + i, kva + i * DISK_SECTOR_SIZE);
		}

		//bitmap의 특정 index 값을 true로 바꾸는 함수
		bitmap_set(swap_table, slot, false);

		anon_page->swap_slot = INVALID_SWAP_SLOT;

		return true;
	}
	// swap slot이 유효하지 않다면
	else
	{
		//swap disk에 저장되어 있지 않은 새로운 page
		memset(page->frame->kva, 0, PGSIZE);

		return true;
	}

}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}
