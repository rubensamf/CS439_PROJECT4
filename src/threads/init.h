#ifndef THREADS_INIT_H
#define THREADS_INIT_H

#include <debug.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "vm/swap.h"

/* Page directory with kernel mappings only. */
extern uint32_t *init_page_dir;

/* Swap Table RDS */
extern struct swap_t *swaptable;

#endif /* threads/init.h */
