#ifndef IRQ_H
#define IRQ_H
#include "isr.h"

typedef void (*isr_t)(registers_t);
void register_interrupt_handler(uint8_t n, isr_t handler);

#endif
