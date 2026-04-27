#include "irq.h"
#include "io.h"

extern void terminal_putchar(char c);

unsigned char keyboard_map[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','\'','ì','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','è','+','\n',
    0, 'a','s','d','f','g','h','j','k','l','ò','à','\\', 0,
    'ù','z','x','c','v','b','n','m',',','.', '-', 0, '*', 0, ' ', 0
};

unsigned char keyboard_map_shift[128] = {
    0, 27, '!','"','£','$','%','&','/','(',')','=','?','^','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','é','*','\n',
    0, 'A','S','D','F','G','H','J','K','L','ç','°','|', 0,
    '§','Z','X','C','V','B','N','M',';',':','_', 0, '*', 0, ' ', 0
};

extern char kbd_buffer[256];
extern int kbd_index;
extern int mouse_x, mouse_y;
extern int mouse_click;

static int shift_pressed = 0;

static void keyboard_callback(registers_t regs) {
    (void)regs;
    uint8_t scancode = inb(0x60); 
    
    if (!(scancode & 0x80)) { // Press
        if (scancode == 0x2A || scancode == 0x36) { // Left/Right Shift
            shift_pressed = 1;
        } else if (scancode == 0x0E) { // Backspace
            if (kbd_index > 0) {
                kbd_index--;
                kbd_buffer[kbd_index] = '\0';
            }
        } else if (scancode == 0x1C) { // Enter
            if (kbd_index < 255) {
                kbd_buffer[kbd_index++] = '\n';
                kbd_buffer[kbd_index] = '\0';
            }
        } else {
            if (kbd_index < 255) {
                char c = shift_pressed ? keyboard_map_shift[scancode] : keyboard_map[scancode];
                if (c) {
                    kbd_buffer[kbd_index++] = c;
                    kbd_buffer[kbd_index] = '\0';
                }
            }
        }
    } else { // Release
        uint8_t rel = scancode & 0x7F;
        if (rel == 0x2A || rel == 0x36) {
            shift_pressed = 0;
        }
    }
}

void init_keyboard() {
    shift_pressed = 0;
    register_interrupt_handler(33, keyboard_callback);
}
