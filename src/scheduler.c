/* File: src/scheduler.c */

#include <stdint.h>
#include "scheduler.h"
#include "console.h"

#define MAX_TASKS 8
#define STACK_SIZE 4096

typedef struct task {
    uint32_t esp;
    uint8_t stack[STACK_SIZE];
} task_t;

static task_t tasks[MAX_TASKS];
static int task_count = 0;
static int current_task = -1;

extern void context_switch(uint32_t *old_esp, uint32_t new_esp);

void scheduler_init(void) {
    task_count = 0;
    current_task = -1;
}

void scheduler_add_task(void (*func)(void)) {
    if (task_count >= MAX_TASKS) {
        print("scheduler: max tasks reached\n");
        return;
    }

    task_t *t = &tasks[task_count];

    uint32_t *stack = (uint32_t*)(t->stack + STACK_SIZE);

    /*
       Stack layout expected by context_switch:

       popa restores:
       EDI, ESI, EBP, ignored ESP, EBX, EDX, ECX, EAX

       Then ret jumps to func.
    */

    *(--stack) = (uint32_t)func;        /* return address for ret */

    *(--stack) = 0x11111111;            /* EAX */
    *(--stack) = 0x22222222;            /* ECX */
    *(--stack) = 0x33333333;            /* EDX */
    *(--stack) = 0x44444444;            /* EBX */
    *(--stack) = 0x55555555;            /* ignored ESP */
    *(--stack) = 0x66666666;            /* EBP */
    *(--stack) = 0x77777777;            /* ESI */
    *(--stack) = 0x88888888;            /* EDI */

    t->esp = (uint32_t)stack;

    task_count++;

    print("scheduler: task added\n");
}

void scheduler_start(void) {
    if (task_count == 0) {
        print("scheduler: no tasks\n");
        return;
    }

    current_task = 0;

    print("scheduler: starting task 0\n");

    uint32_t dummy_old_esp = 0;
    context_switch(&dummy_old_esp, tasks[0].esp);
}

void scheduler_switch(void) {
    if (task_count == 0) {
        return;
    }

    int old_task = current_task;

    current_task++;
    if (current_task >= task_count) {
        current_task = 0;
    }

    if (old_task < 0) {
        scheduler_start();
        return;
    }

    context_switch(&tasks[old_task].esp, tasks[current_task].esp);
}