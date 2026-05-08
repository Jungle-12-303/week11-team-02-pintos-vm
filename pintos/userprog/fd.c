#include "userprog/fd.h"
#include "threads/malloc.h"
#include "lib/debug.h"
#include "filesys/file.h"
#include <limits.h>
#include <stdbool.h>

static int fd_expaned (struct fd_table *fdt, const size_t new_size);

struct fd_table *
fd_table_init (void) {
    struct fd_table *fdt = malloc (sizeof (struct fd_table));
    if (fdt == NULL) {
        // TODO: calloc 예외처리
        ASSERT (fdt != NULL);
        return NULL;
    }
    fdt->fds = calloc (FD_ENTRY_N, sizeof (struct fd_entry *));
    if (fdt->fds == NULL) {
        // TODO: calloc 예외처리
        free(fdt);
        ASSERT (false);
        return NULL;
    }
    fdt->size = FD_ENTRY_N;
    struct fd_entry *fd_stdin = malloc(sizeof (struct fd_entry));
    struct fd_entry *fd_stdout = malloc(sizeof (struct fd_entry));
    if (fd_stdin == NULL || fd_stdout == NULL) {
        // TODO: malloc 예외처리
        free(fdt->fds);
        free(fdt);
        free(fd_stdin);
        free(fd_stdout);
        ASSERT (false);
        return NULL;
    }

    fd_stdin->type = FD_STDIN;
    fd_stdin->file = NULL;

    fd_stdout->type = FD_STDOUT;
    fd_stdout->file = NULL;

    fdt->fds[0] = fd_stdin;
    fdt->fds[1] = fd_stdout;
    return fdt;
}

/* 할당 가능한 fd를 반환합니다. */
int
fd_find_blank (struct fd_table *fdt) {

    ASSERT (fdt != NULL);

    for(size_t i = 0; i < fdt->size; i++) {
        if(fdt->fds[i] == NULL) {
            if (i > INT_MAX) {
                return -1;
            }
            return (int) i;
        }
    }
    return fd_expaned(fdt, fdt->size << 1);
}

/* 사이즈를 NEW_SIZE로 확장합니다. 만약 더 작은 사이즈로 
   expaned하려고 하면, -1를 리턴합니다. */
static int
fd_expaned (struct fd_table *fdt, const size_t new_size) {

    ASSERT (fdt != NULL);

    if(fdt->size < new_size) {
        if (fdt->size > INT_MAX) {
            return -1;
        }
        struct fd_entry **tmp = calloc (new_size, sizeof (struct fd_entry *));
        if (tmp == NULL) {
            // TODO:예외처리
            ASSERT (tmp != NULL);
            return -1;
        }
        for(size_t i = 0; i < fdt->size; i++) {
            tmp[i] = fdt->fds[i];
        }
        free(fdt->fds);
        fdt->fds = tmp;
        size_t ret = fdt->size;
        fdt->size = new_size;
        return (int) ret;
    } else {
        return -1;
    }
}

void
fd_entry_free (struct fd_table *fdt, size_t index) {
    // TODO: e 안의 요소 free해야할 것 있으면 하기
    file_close (fdt->fds[index]->file);
    free (fdt->fds[index]);
    fdt->fds[index] = NULL;
}

/* fd_table을 할당 해제합니다. 그 안의 요소들도 해제합니다. */
void
fd_table_free (struct fd_table *fdt) {
    /* TODO: fd_table 안의 멤버들도 free 해줘야함. */
    if (fdt == NULL) {
        return;
    }
    for(size_t i = 0; i < fdt->size; i++) {
        if(fdt->fds[i] != NULL) {
            fd_entry_free(fdt, i);
        }
    }
    free(fdt->fds);
    free(fdt);
}

/**
 * fd_entry를 조회하는 함수
 */
struct fd_entry*
fd_get_entry (struct fd_table *fdt, int fd){
    if(fdt == NULL){ // 잘못된 테이블을 전달시
        // 이 함수 자체의 반환값이 "struct fd_entry*"이기 때문에 
        // 잘못된 경우 -1이 아닌 NULL을 반환
        return NULL;
    }

    if(((size_t)fd >= fdt->size) || (fd < 0)){ // 유효한 fd 범위가 아닐때
        return NULL; 
    }

    struct fd_entry* fde = fdt->fds[fd];
    return fde;
}

/**
 * fdt의 fd가 유효한지 검사합니다. 유효하지 않으면 false를 반환하고,
 * 유효하다면 true를 반환합니다.
 */
bool
fd_is_valid (struct fd_table *fdt, int fd) {
    ASSERT (fdt != NULL);
    ASSERT (fdt->fds != NULL);
    if(fd < 0 || fd >= fdt->size) return false;
    return fdt->fds[fd] != NULL;
}


/**
 * fdt에 비어있는 fd를 찾아서 file entry에 파일 할당하는 함수
 */
int
fd_table_add_file (struct fd_table *fdt, struct file *file){
    ASSERT(fdt != NULL); // fdt가 NULL 이면 패닉
    ASSERT(file != NULL); // file이 NULL 이면 패닉

    int fd;
    if( (fd = fd_find_blank(fdt)) == -1){ // 빈 fd 찾기 + 예외처리
        return -1;
    }
    
    struct fd_entry *fde;
    // 메모리에 fd_endtry 만큼의 공간 할당 요청 + 예외처리
    if( (fde = malloc(sizeof (struct fd_entry))) == NULL ){
        return -1;
    } 
    
    fde->type = FD_FILE; // fd_entry 구조체의 file_type에 파일 타입 넣기
    fde->file = file; // fd_entry 구조체에 파일 주소 넣기

    fdt->fds[fd] = fde; // fd_table에 fd_entry 주소 넣기

    return fd;
}
