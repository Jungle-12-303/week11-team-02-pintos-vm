/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "lib/kernel/bitmap.h"
#include "threads/vaddr.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static struct bitmap* swap_bitmap;
static struct lock swap_lock;
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
	page->anon.swap_slot = -1;

	return true;
}

/* Swap in the page by read contents from the swap disk. */
// TODO: [5]
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
}

/* Swap out the page by writing contents to the swap disk. */
// TODO: [6]
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
// TODO: [7]
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}
