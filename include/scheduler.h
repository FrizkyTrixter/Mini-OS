#pragma once
#include <stdint.h>

void scheduler_init(void);
void scheduler_add_task(void (*func)(void));
void scheduler_start(void);
void scheduler_switch(void);