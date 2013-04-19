#ifndef USERPROG_FDT_H
#define USERPROG_FDT_H

#define FDT_MAX_FILES 128

typedef struct file ** fdt_t;

int fd_create(struct file *file);
struct file *fd_get_file(int fd);
struct file *fd_remove(int fd);

void fdt_destroy(fdt_t fdt);
fdt_t fdt_init(void);

#endif
