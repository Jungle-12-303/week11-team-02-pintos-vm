#ifndef __USERPROG_PROCESS_CHILD
#define __USERPROG_PROCESS_CHILD

#include "threads/thread.h"
#include "threads/synch.h"

typedef int tid_t;

struct child_status {
    tid_t tid;
    int exit_code;
    bool exited;
    bool waited;
    struct semaphore wait_sema;
    struct list_elem elem;
};

void init_process_status_list();
struct child_status *get_child_status(const tid_t tid);
bool child_status_insert(const tid_t tid);
void child_status_sema_down(struct child_status *status);
void child_status_sema_up(struct child_status *status);
void destory_child_statuses();

#endif /* userprog/process_child.h */