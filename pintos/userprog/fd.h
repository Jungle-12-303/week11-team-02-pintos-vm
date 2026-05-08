#ifndef USERPROG_FD_H
#define USERPROG_FD_H

#include <stddef.h>
#include <stdbool.h>

#define FD_ENTRY_N (1 << 8)

struct file;

enum fd_type {
    FD_STDIN,
    FD_STDOUT,
    FD_FILE,
};

struct fd_entry {
    enum fd_type type;
    struct file* file;
};

struct fd_table {
    struct fd_entry **fds;
    size_t size;
};

struct fd_table *fd_table_init (void);
int fd_find_blank (struct fd_table *fdt);
void fd_entry_free (struct fd_table *fdt, size_t index);
void fd_table_free (struct fd_table *fdt);
struct fd_entry* fd_get_entry (struct fd_table *fdt, int fd);
int fd_table_add_file (struct fd_table *fdt, struct file *file);
bool fd_is_valid (struct fd_table *fdt, int fd);

#endif /* USERPROG_FD_H */
