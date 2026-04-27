#include "isr.h"

extern void terminal_writestring(const char* data);

void isr_handler(registers_t regs) {
    terminal_writestring("Eccezione Hardware (Interrupt) Ricevuta!\n");

    while(1);
}
