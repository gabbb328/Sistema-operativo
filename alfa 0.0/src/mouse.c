#include "mouse.h"
#include "irq.h"
#include "io.h"

int mouse_x = 400;
int mouse_y = 300;
int mouse_click = 0;

/* Limiti dinamici impostati dalla kernel_main dopo aver letto la risoluzione */
int mouse_max_x = 799;
int mouse_max_y = 599;

static uint8_t mouse_cycle = 0;
static int8_t mouse_byte[3];

static inline void mouse_wait(uint8_t a_type) {
    uint32_t timeout = 100000;
    if (a_type == 0) {
        while (timeout--) if ((inb(0x64) & 1) == 1) return;
    } else {
        while (timeout--) if ((inb(0x64) & 2) == 0) return;
    }
}

static inline void mouse_write(uint8_t write) {
    mouse_wait(1);
    outb(0x64, 0xD4);
    mouse_wait(1);
    outb(0x60, write);
}

static inline uint8_t mouse_read(void) {
    mouse_wait(0);
    return inb(0x60);
}

static void mouse_callback(registers_t regs) {
    (void)regs;

    uint8_t status = inb(0x64);
    if (!(status & 0x01) || !(status & 0x20)) return; // No data or not mouse data

    int8_t mouse_in = inb(0x60);

    switch (mouse_cycle) {
        case 0:
            mouse_byte[0] = mouse_in;
            if (mouse_in & 0x08) mouse_cycle++;
            break;
        case 1:
            mouse_byte[1] = mouse_in;
            mouse_cycle++;
            break;
        case 2:
            mouse_byte[2] = mouse_in;
            mouse_cycle = 0;

            mouse_click = (mouse_byte[0] & 0x01) ? 1 : 0;

            int x_mov = (int)mouse_byte[1];
            int y_mov = (int)mouse_byte[2];

            /* Handle sign bits */
            if (mouse_byte[0] & 0x10) x_mov -= 256;
            if (mouse_byte[0] & 0x20) y_mov -= 256;

            mouse_x += x_mov;
            mouse_y -= y_mov;

            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x > mouse_max_x) mouse_x = mouse_max_x;
            if (mouse_y > mouse_max_y) mouse_y = mouse_max_y;
            break;
    }
}

void init_mouse() {
    uint8_t status;

    // Enable auxiliary device
    mouse_wait(1);
    outb(0x64, 0xA8);

    // Enable interrupts
    mouse_wait(1);
    outb(0x64, 0x20);
    mouse_wait(0);
    status = (inb(0x60) | 2); // Enable IRQ 12
    status &= ~0x20;         // Enable mouse clock

    mouse_wait(1);
    outb(0x64, 0x60);
    mouse_wait(1);
    outb(0x60, status);

    // Set Default Settings
    mouse_write(0xF6);
    mouse_read();

    // Enable Data Reporting
    mouse_write(0xF4);
    mouse_read();

    register_interrupt_handler(44, mouse_callback);
}
