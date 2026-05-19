#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
struct page;
enum vm_type;

// page가 swap되었는데, 이걸 기록해둬야 함.
struct anon_page {
    enum vm_type type;
    bool swap_status; // 어디에 저장을 해뒀는가?
    size_t swap_slot; // 어디에 저장해뒀는가
};

void vm_anon_init (void);
bool anon_initializer (struct page *page, enum vm_type type, void *kva);

#endif
