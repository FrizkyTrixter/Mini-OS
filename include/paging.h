// include/paging.h
#ifndef PAGING_H
#define PAGING_H
#include <stdint.h>

void init_paging(void);

void map_page(uint32_t va, uint32_t pa, uint32_t flags);

#endif // PAGING_H

