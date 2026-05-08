#include "process_child.h"
#include <debug.h>
#include "threads/malloc.h"

static struct list child_status_list;

/* child_status 리스트를 관리하는 child_status_list를
   사용하기 전, 초기화합니다. */
void 
init_process_status_list () {
    list_init(&child_status_list);
}

struct child_status *
get_child_status (const tid_t tid) {
    struct list_elem *e;
    if(list_empty(&child_status_list)) {
        return NULL;
    }
    for(e = list_begin(&child_status_list); 
        e != list_end(&child_status_list); 
        e = list_next(e)) {
        struct child_status *status = list_entry(e, struct child_status, elem);
        if(status->tid == tid) {
            return status;
        }
    }
    return NULL;
}

/* child_status_list의 tid는 유일해야합니다. 만약 이미 존재하는
   tid를 삽입하려고 하면, false를 리턴합니다. 성공적으로 list에
   삽입되면, true를 리턴합니다. */
bool 
child_status_insert (const tid_t tid) {
    struct child_status *status = get_child_status(tid);
    if(status == NULL) {
        struct child_status *process = malloc(sizeof(struct child_status)); /* must be free */
        if (process == NULL) {
            return false;
        } else {
            process->tid = tid;
            process->exit_code = -1;
            process->exited = false;
            process->waited = true;
            sema_init(&process->wait_sema, 0); /* for process_wait */
            list_push_back(&child_status_list, &process->elem);
            return true;
        }
    } else {
        return false;
    }
}

void 
child_status_sema_down (struct child_status *status) {
    ASSERT (status != NULL)
    sema_down(&status->wait_sema);
}

void 
child_status_sema_up (struct child_status *status) {
    ASSERT (status != NULL)
    sema_up(&status->wait_sema);
}

/* child_status_list의 요소들을 free합니다. */
void 
destory_child_statuses () {
    while(!list_empty(&child_status_list)) {
        struct child_status *status = list_entry(list_pop_front(&child_status_list), struct child_status, elem);
        free(status);
    }
}
