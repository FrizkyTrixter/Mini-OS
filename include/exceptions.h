/* File: include/exceptions.h */
#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <stdint.h>

/* Install IDT entries 0..31 to the exception ISR stubs (isr0..isr31). */
void exceptions_install(void);

/* Called by isr_exceptions.s for every CPU exception. */
void exception_handler_c(uint32_t vec, uint32_t errcode);

#endif /* EXCEPTIONS_H */
