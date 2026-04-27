#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* =========================================================
 *  VGA text-mode legacy (non usato in framebuffer mode)
 * ========================================================= */
enum vga_color {
    VGA_COLOR_BLACK = 0, VGA_COLOR_BLUE = 1, VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,  VGA_COLOR_RED  = 4, VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6, VGA_COLOR_LIGHT_GREY = 7, VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9, VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11, VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13, VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15,
};
static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) { return fg | bg << 4; }
static inline uint16_t vga_entry(unsigned char uc, uint8_t color) { return (uint16_t)uc | (uint16_t)color << 8; }

size_t terminal_row, terminal_column;
uint8_t terminal_color;
uint16_t* terminal_buffer;

void terminal_initialize(void) {
    terminal_row = 0; terminal_column = 0;
    terminal_color = vga_entry_color(VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    terminal_buffer = (uint16_t*)0xB8000;
    for (size_t y = 0; y < 25; y++)
        for (size_t x = 0; x < 80; x++)
            terminal_buffer[y * 80 + x] = vga_entry(' ', terminal_color);
}
void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) {
    terminal_buffer[y * 80 + x] = vga_entry(c, color);
}
void terminal_putchar(char c) {
    terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
    if (++terminal_column == 80) { terminal_column = 0; if (++terminal_row == 25) terminal_row = 0; }
}
void terminal_write(const char* data, size_t size) { for (size_t i = 0; i < size; i++) terminal_putchar(data[i]); }
void terminal_writestring(const char* data) {
    size_t l = 0; while (data[l]) l++;
    terminal_write(data, l);
}

/* =========================================================
 *  Includes
 * ========================================================= */
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "keyboard.h"
#include "multiboot.h"
#include "paging.h"
#include "kmalloc.h"
#include "mouse.h"
#include "font.h"
#include "io.h"

/* =========================================================
 *  Palette moderna – dark / accent blu-viola
 * =========================================================
 *  DARK_BG      sfondo desktop scuro (quasi nero blu)
 *  PANEL_BG     barre / pannelli  (grigio scuro traslucente-fake)
 *  WIN_BG       sfondo finestra
 *  TITLE_BG     barra titolo finestra
 *  TITLE_FG     testo titolo
 *  ACCENT       blu accent principale
 *  ACCENT2      viola accent secondario
 *  TEXT_PRIMARY testo principale
 *  TEXT_MUTED   testo secondario
 *  BTN_OK       pulsante conferma
 *  BTN_DANGER   pulsante pericolo / chiudi
 *  BTN_WARN     pulsante avviso / minimizza
 *  BTN_OK_G     pulsante verde / massimizza
 *  SHADOW       ombra finestre
 *  INPUT_BG     sfondo campi input
 *  CURSOR_COL   cursore freccia
 *  CURSOR_ACC   cursore – parte chiara
 * ========================================================= */
#define C_DARK_BG      0x0D1117
#define C_PANEL_BG     0x161B22
#define C_PANEL_BORDER 0x21262D
#define C_WIN_BG       0x1C2128
#define C_WIN_BG2      0x22272E
#define C_TITLE_BG     0x13151A
#define C_TITLE_FG     0xCDD9E5
#define C_ACCENT       0x388BFD
#define C_ACCENT_HOV   0x58A6FF
#define C_ACCENT2      0x8250DF
#define C_TEXT_PRI     0xCDD9E5
#define C_TEXT_MUT     0x768390
#define C_BTN_CLOSE    0xFF5F57
#define C_BTN_MIN      0xFFBD2E
#define C_BTN_MAX      0x28CA41
#define C_SHADOW       0x000000
#define C_INPUT_BG     0x0D1117
#define C_INPUT_BOR    0x388BFD
#define C_CURSOR       0xFFFFFF
#define C_CURSOR_SH    0x000000

/* =========================================================
 *  Primitive di disegno
 * ========================================================= */
int screen_width = 800;
int screen_height = 600;
static int fb_pitch_g = 0;

static inline void draw_pixel(uint32_t* fb, int x, int y, uint32_t color, int pitch) {
    if (x < 0 || x >= screen_width || y < 0 || y >= screen_height) return;
    fb[y * (pitch / 4) + x] = color;
}

static void draw_rect(uint32_t* fb, int x, int y, int w, int h, uint32_t color, int pitch) {
    for (int i = 0; i < h; i++)
        for (int j = 0; j < w; j++)
            draw_pixel(fb, x + j, y + i, color, pitch);
}

/* Mix alpha semplificato (0–255) */
static inline uint32_t blend(uint32_t dst, uint32_t src, int a) {
    uint8_t r = ((dst >> 16 & 0xFF) * (255 - a) + (src >> 16 & 0xFF) * a) / 255;
    uint8_t g = ((dst >>  8 & 0xFF) * (255 - a) + (src >>  8 & 0xFF) * a) / 255;
    uint8_t b = ((dst       & 0xFF) * (255 - a) + (src       & 0xFF) * a) / 255;
    return (r << 16) | (g << 8) | b;
}

static void draw_rect_alpha(uint32_t* fb, int x, int y, int w, int h,
                             uint32_t color, int alpha, int pitch) {
    for (int i = 0; i < h; i++) {
        int py = y + i;
        if (py < 0 || py >= screen_height) continue;
        for (int j = 0; j < w; j++) {
            int px = x + j;
            if (px < 0 || px >= screen_width) continue;
            int idx = py * (pitch / 4) + px;
            fb[idx] = blend(fb[idx], color, alpha);
        }
    }
}

static void draw_rounded_rect(uint32_t* fb, int x, int y, int w, int h,
                               int r, uint32_t color, int pitch) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            int draw = 1;
            if      (i < r && j < r)           { int dx = r-j-1, dy = r-i-1; if (dx*dx+dy*dy > r*r) draw=0; }
            else if (i < r && j >= w-r)        { int dx = j-(w-r), dy = r-i-1; if (dx*dx+dy*dy > r*r) draw=0; }
            else if (i >= h-r && j < r)        { int dx = r-j-1, dy = i-(h-r); if (dx*dx+dy*dy > r*r) draw=0; }
            else if (i >= h-r && j >= w-r)     { int dx = j-(w-r), dy = i-(h-r); if (dx*dx+dy*dy > r*r) draw=0; }
            if (draw) draw_pixel(fb, x+j, y+i, color, pitch);
        }
    }
}

/* Rettangolo con solo angoli superiori arrotondati */
static void draw_top_rounded_rect(uint32_t* fb, int x, int y, int w, int h,
                                   int r, uint32_t color, int pitch) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            int draw = 1;
            if (i < r && j < r)       { int dx=r-j-1, dy=r-i-1; if (dx*dx+dy*dy > r*r) draw=0; }
            else if (i < r && j>=w-r) { int dx=j-(w-r), dy=r-i-1; if (dx*dx+dy*dy > r*r) draw=0; }
            if (draw) draw_pixel(fb, x+j, y+i, color, pitch);
        }
    }
}

static void draw_char(uint32_t* fb, char c, int x, int y,
                      uint32_t fg, uint32_t bg, int pitch) {
    if ((unsigned char)c >= 128) return;
    char* glyph = font8x8_basic[(int)c];
    for (int cy = 0; cy < 8; cy++)
        for (int cx = 0; cx < 8; cx++)
            if ((glyph[cy] >> (7 - cx)) & 1)
                draw_pixel(fb, x+cx, y+cy, fg, pitch);
            else if (bg != 0xFFFFFFFF)
                draw_pixel(fb, x+cx, y+cy, bg, pitch);
}

static void draw_string(uint32_t* fb, const char* str, int x, int y,
                        uint32_t fg, uint32_t bg, int pitch) {
    int ox = x;
    while (*str) {
        if (*str == '\n') {
            x = ox;
            y += 10;
        } else {
            draw_char(fb, *str, x, y, fg, bg, pitch);
            x += 8;
        }
        str++;
    }
}

static int str_len(const char* s) { int n=0; while(s[n]) n++; return n; }

static void draw_string_centered(uint32_t* fb, const char* str, int cx, int y,
                                  uint32_t fg, uint32_t bg, int pitch) {
    int px = cx - (str_len(str) * 8) / 2;
    draw_string(fb, str, px, y, fg, bg, pitch);
}

/* Linea orizzontale sottile (separatore) */
static void draw_hline(uint32_t* fb, int x, int y, int w, uint32_t color, int pitch) {
    for (int i = 0; i < w; i++) draw_pixel(fb, x+i, y, color, pitch);
}

/* Gradiente verticale su un rettangolo */
static void draw_vgradient(uint32_t* fb, int x, int y, int w, int h,
                            uint32_t top, uint32_t bot, int pitch) {
    for (int i = 0; i < h; i++) {
        uint8_t tr=(top>>16)&0xFF, tg=(top>>8)&0xFF, tb=top&0xFF;
        uint8_t br=(bot>>16)&0xFF, bg2=(bot>>8)&0xFF, bb=bot&0xFF;
        uint32_t c = (((tr*(h-1-i)+br*i)/(h-1))<<16)|
                     (((tg*(h-1-i)+bg2*i)/(h-1))<<8)|
                      ((tb*(h-1-i)+bb*i)/(h-1));
        for (int j = 0; j < w; j++) draw_pixel(fb, x+j, y+i, c, pitch);
    }
}

/* =========================================================
 *  Cursore a freccia pixel-art (16×16)
 * ========================================================= */
/* 1 = corpo bianco, 2 = bordo nero */
static const uint8_t arrow_cursor[16][16] = {
    {1,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,2,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,1,2,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,1,1,2,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,1,1,1,2,0,0,0,0,0,0,0,0,0,0},
    {1,1,1,1,1,1,2,0,0,0,0,0,0,0,0,0},
    {1,1,1,1,1,1,1,2,0,0,0,0,0,0,0,0},
    {1,1,1,1,1,1,1,1,2,0,0,0,0,0,0,0},
    {1,1,1,1,1,1,1,1,1,2,0,0,0,0,0,0},
    {1,1,1,1,1,1,2,2,2,2,0,0,0,0,0,0},
    {1,1,1,2,1,1,2,0,0,0,0,0,0,0,0,0},
    {1,1,2,0,2,1,1,2,0,0,0,0,0,0,0,0},
    {1,2,0,0,0,2,1,1,2,0,0,0,0,0,0,0},
    {2,0,0,0,0,0,2,1,1,2,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,2,1,1,2,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,2,2,0,0,0,0,0,0},
};

static void draw_cursor(uint32_t* fb, int x, int y, int pitch) {
    for (int cy = 0; cy < 16; cy++) {
        for (int cx = 0; cx < 16; cx++) {
            int px = x+cx, py = y+cy;
            if (px < 0 || py < 0) continue;
            uint8_t p = arrow_cursor[cy][cx];
            if (p == 1) draw_pixel(fb, px, py, C_CURSOR,   pitch);
            if (p == 2) draw_pixel(fb, px, py, C_CURSOR_SH, pitch);
        }
    }
}

/* =========================================================
 *  Strutture dati OS
 * ========================================================= */
typedef struct {
    int x, y, w, h;
    char title[40];
    int minimized;
} Window;

Window task_manager = { 60,  80, 310, 210, "Monitor di Sistema", 0 };
Window notepad_app  = { 420, 90, 380, 260, "Terminale",          0 };

char kbd_buffer[256] = {0};
int  kbd_index = 0;

/* =========================================================
 *  Utilità
 * ========================================================= */
static void str_copy(char* d, const char* s, int max) {
    for (int i = 0; i < max-1; i++) { d[i]=s[i]; if (!s[i]) break; }
    d[max-1]='\0';
}
static int str_eq(const char* a, const char* b) {
    while (*a && *b) { if (*a!=*b) return 0; a++; b++; }
    return (*a==*b);
}

uint8_t get_RTC(int reg) { outb(0x70, reg); return inb(0x71); }
void get_rtc_time(int* h, int* m, int* s) {
    *s = ((get_RTC(0x00)>>4)*10)+(get_RTC(0x00)&0x0F);
    *m = ((get_RTC(0x02)>>4)*10)+(get_RTC(0x02)&0x0F);
    *h = ((get_RTC(0x04)>>4)*10)+(get_RTC(0x04)&0x0F);
}

/* =========================================================
 *  Rendering componenti UI
 * ========================================================= */

/* Finestra con ombra, titlebar scura, contenuto */
static void render_window(uint32_t* fb, Window* w, int pitch) {
    if (w->minimized) return;

    /* Ombra */
    draw_rect_alpha(fb, w->x+6, w->y+6, w->w, w->h, C_SHADOW, 120, pitch);

    /* Corpo finestra */
    draw_rounded_rect(fb, w->x, w->y, w->w, w->h, 10, C_WIN_BG, pitch);

    /* Titlebar */
    draw_top_rounded_rect(fb, w->x, w->y, w->w, 28, 10, C_TITLE_BG, pitch);
    draw_rect(fb, w->x, w->y+18, w->w, 10, C_TITLE_BG, pitch); /* riempimento basso titlebar */

    /* Separatore titlebar */
    draw_hline(fb, w->x, w->y+28, w->w, C_PANEL_BORDER, pitch);

    /* Pulsanti semaforo */
    draw_rounded_rect(fb, w->x+10, w->y+8, 12, 12, 6, C_BTN_CLOSE, pitch);
    draw_rounded_rect(fb, w->x+28, w->y+8, 12, 12, 6, C_BTN_MIN,   pitch);
    draw_rounded_rect(fb, w->x+46, w->y+8, 12, 12, 6, C_BTN_MAX,   pitch);

    /* Titolo centrato */
    draw_string_centered(fb, w->title, w->x + w->w/2, w->y+10, C_TITLE_FG, 0xFFFFFFFF, pitch);
}

/* Pulsante con hover */
static void render_button(uint32_t* fb, int x, int y, int w, int h,
                           const char* label, int hovered, int pitch) {
    uint32_t col = hovered ? C_ACCENT_HOV : C_ACCENT;
    draw_rounded_rect(fb, x, y, w, h, 8, col, pitch);
    draw_string_centered(fb, label, x + w/2, y + (h-8)/2, 0xFFFFFF, 0xFFFFFFFF, pitch);
}

/* Campo input con bordo accent quando attivo */
static void render_input(uint32_t* fb, int x, int y, int w, int h,
                          const char* text, int active, int pitch) {
    draw_rounded_rect(fb, x, y, w, h, 6, C_INPUT_BG, pitch);
    /* Bordo */
    uint32_t bor = active ? C_INPUT_BOR : C_PANEL_BORDER;
    draw_hline(fb, x,   y,       w, bor, pitch);
    draw_hline(fb, x,   y+h-1,   w, bor, pitch);
    for (int i=0;i<h;i++) {
        draw_pixel(fb, x,   y+i, bor, pitch);
        draw_pixel(fb, x+w-1, y+i, bor, pitch);
    }
    draw_string(fb, text, x+8, y+(h-8)/2, C_TEXT_PRI, 0xFFFFFFFF, pitch);
    /* Cursore lampeggiante (sempre visibile) */
    if (active) {
        int cx = x + 8 + str_len(text)*8;
        draw_rect(fb, cx, y+4, 2, h-8, C_ACCENT, pitch);
    }
}

/* =========================================================
 *  Sfondo desktop – gradiente orizzontale sofisticato
 * ========================================================= */
static void draw_desktop_bg(uint32_t* fb, int width, int height, int pitch) {
    /* Gradiente diagonale scuro */
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            /* mix verticale */
            uint8_t r = 0x0D + (y * 4 / height);
            uint8_t g = 0x11 + (y * 6 / height);
            uint8_t b = 0x17 + (y * 12 / height) + (x * 4 / width);
            draw_pixel(fb, x, y, ((uint32_t)r<<16)|((uint32_t)g<<8)|b, pitch);
        }
    }
    /* Lieve "spotlight" al centro in alto */
    int cx = width/2, cy = 0;
    for (int r = 300; r > 0; r -= 2) {
        draw_rect_alpha(fb, cx-r, cy-r, r*2, r*2, 0x1A3A6A, (300-r)/10, pitch);
    }
}

/* =========================================================
 *  Dock stile macOS
 * ========================================================= */
#define DOCK_ICON_SIZE 44
#define DOCK_PAD       10
#define DOCK_ICONS     4

static void draw_dock(uint32_t* fb, int width, int height, int pitch,
                      int hover_icon) {
    int dock_item_w = DOCK_ICON_SIZE + DOCK_PAD;
    int dock_w = DOCK_ICONS * dock_item_w + DOCK_PAD;
    int dock_h = DOCK_ICON_SIZE + DOCK_PAD * 2;
    int dock_x = width/2 - dock_w/2;
    int dock_y = height - dock_h - 8;

    /* Sfondo dock glass-like */
    draw_rect_alpha(fb, dock_x-2, dock_y-2, dock_w+4, dock_h+4,
                    C_PANEL_BORDER, 180, pitch);
    draw_rounded_rect(fb, dock_x, dock_y, dock_w, dock_h, 14, C_PANEL_BG, pitch);
    /* Bordo luminoso sopra */
    draw_hline(fb, dock_x, dock_y, dock_w, 0x2D333B, pitch);

    /* Icone */
    static const uint32_t icon_colors[DOCK_ICONS] = {
        0x388BFD, /* home/finder – blu */
        0xFF5F57, /* system monitor – rosso */
        0x28CA41, /* terminale – verde */
        0xFFBD2E, /* settings – giallo */
    };
    static const char* icon_labels[DOCK_ICONS] = { "Home", "Sist.", "Term.", "Impo." };

    for (int i = 0; i < DOCK_ICONS; i++) {
        int ix = dock_x + DOCK_PAD + i * dock_item_w;
        int iy = dock_y + DOCK_PAD;
        int sz = DOCK_ICON_SIZE;
        int hov = (hover_icon == i);

        /* Hover lift */
        if (hov) iy -= 4;

        /* Ombra icona */
        draw_rect_alpha(fb, ix+3, iy+3, sz, sz, 0x000000, 80, pitch);
        /* Icona */
        draw_rounded_rect(fb, ix, iy, sz, sz, 10, icon_colors[i], pitch);
        /* Highlight interno icona */
        draw_rect_alpha(fb, ix+4, iy+4, sz-8, 8, 0xFFFFFF, 40, pitch);

        /* Label sotto dock */
        if (hov) {
            int lx = ix + sz/2 - str_len(icon_labels[i])*4;
            draw_string(fb, icon_labels[i], lx, dock_y + dock_h + 2,
                        C_TEXT_MUT, 0xFFFFFFFF, pitch);
        }
    }
    (void)dock_w; (void)dock_h; (void)dock_x;
}

static int dock_icon_at(int mx, int my, int width, int height) {
    int dock_item_w = DOCK_ICON_SIZE + DOCK_PAD;
    int dock_w = DOCK_ICONS * dock_item_w + DOCK_PAD;
    int dock_h = DOCK_ICON_SIZE + DOCK_PAD * 2;
    int dock_x = width/2 - dock_w/2;
    int dock_y = height - dock_h - 8;
    if (my < dock_y || my > dock_y + dock_h) return -1;
    for (int i = 0; i < DOCK_ICONS; i++) {
        int ix = dock_x + DOCK_PAD + i * dock_item_w;
        if (mx >= ix && mx <= ix + DOCK_ICON_SIZE) return i;
    }
    return -1;
}

/* =========================================================
 *  kernel_main
 * ========================================================= */
int os_state  = 0;
char os_username[32] = "";
char os_password[32] = "";

void kernel_main(uint32_t multiboot_magic, multiboot_info_t* mbd) {
    init_gdt();
    init_idt();
    PIC_remap(0x20, 0x28);
    init_kmalloc();
    asm volatile("sti");
    init_keyboard();
    init_mouse();

    if (multiboot_magic != 0x2BADB002) return;
    if (!(mbd->flags & (1 << 12))) return;

    uint32_t* frontbuffer = (uint32_t*)mbd->framebuffer_addr_low;
    int pitch  = mbd->framebuffer_pitch;
    int width  = mbd->framebuffer_width;
    int height = mbd->framebuffer_height;

    screen_width  = width;
    screen_height = height;

    /* Aggiorna limiti mouse alla risoluzione reale */
    mouse_max_x = width  - 1;
    mouse_max_y = height - 1;
    mouse_x = width  / 2;
    mouse_y = height / 2;

    uint32_t* backbuffer = (uint32_t*)kmalloc(width * height * 4);

    int setup_step = 0;

    while (1) {
        /* === Sfondo desktop === */
        draw_desktop_bg(backbuffer, width, height, pitch);

        /* -------------------------------------------------------
         *  STATO 0 – Setup (scegli username + password)
         * ------------------------------------------------------- */
        if (os_state == 0) {
            int box_w = 420, box_h = 320;
            int box_x = width/2  - box_w/2;
            int box_y = height/2 - box_h/2;

            /* Ombra modale */
            draw_rect_alpha(backbuffer, box_x+8, box_y+8, box_w, box_h,
                            C_SHADOW, 140, pitch);
            draw_rounded_rect(backbuffer, box_x, box_y, box_w, box_h, 12, C_WIN_BG, pitch);

            /* Header modale */
            draw_top_rounded_rect(backbuffer, box_x, box_y, box_w, 54, 12, C_TITLE_BG, pitch);
            draw_rect(backbuffer, box_x, box_y+44, box_w, 10, C_TITLE_BG, pitch);
            draw_hline(backbuffer, box_x, box_y+54, box_w, C_PANEL_BORDER, pitch);

            /* Logo testuale con accent */
            draw_string_centered(backbuffer, "Gafite OS", width/2, box_y+14,
                                 C_ACCENT, 0xFFFFFFFF, pitch);
            draw_string_centered(backbuffer, "Setup iniziale", width/2, box_y+30,
                                 C_TEXT_MUT, 0xFFFFFFFF, pitch);

            if (setup_step == 0) {
                draw_string(backbuffer, "Scegli il nome utente:",
                            box_x+30, box_y+75, C_TEXT_MUT, 0xFFFFFFFF, pitch);
                render_input(backbuffer, box_x+30, box_y+92, box_w-60, 32,
                             kbd_buffer, 1, pitch);

                int bx=box_x+box_w/2-55, by=box_y+box_h-60;
                int hov=(mouse_x>bx&&mouse_x<bx+110&&mouse_y>by&&mouse_y<by+34);
                render_button(backbuffer, bx, by, 110, 34, "Avanti ->", hov, pitch);

                static int pc0=0;
                if (!pc0 && mouse_click && hov && kbd_index > 0) {
                    str_copy(os_username, kbd_buffer, 32);
                    kbd_buffer[0]=0; kbd_index=0; setup_step=1;
                }
                pc0=mouse_click;

            } else {
                char prompt[64] = "Ciao, ";
                int pl=6;
                for(int i=0;os_username[i]&&i<20;i++) prompt[pl++]=os_username[i];
                prompt[pl++]='!'; prompt[pl]=0;
                draw_string(backbuffer, prompt,
                            box_x+30, box_y+70, C_ACCENT, 0xFFFFFFFF, pitch);
                draw_string(backbuffer, "Scegli una password:",
                            box_x+30, box_y+92, C_TEXT_MUT, 0xFFFFFFFF, pitch);

                char stars[64]="";
                for(int z=0;z<kbd_index&&z<63;z++) stars[z]='*';
                render_input(backbuffer, box_x+30, box_y+110, box_w-60, 32,
                             stars, 1, pitch);

                int bx=box_x+box_w/2-55, by=box_y+box_h-60;
                int hov=(mouse_x>bx&&mouse_x<bx+110&&mouse_y>by&&mouse_y<by+34);
                render_button(backbuffer, bx, by, 110, 34, "Fine  ->", hov, pitch);

                static int pc1=0;
                if (!pc1 && mouse_click && hov) {
                    str_copy(os_password, kbd_buffer, 32);
                    kbd_buffer[0]=0; kbd_index=0; os_state=1;
                }
                pc1=mouse_click;
            }

        /* -------------------------------------------------------
         *  STATO 1 – Desktop
         * ------------------------------------------------------- */
        } else if (os_state == 1) {

            /* ESC → schermata di blocco */
            if (kbd_buffer[kbd_index > 0 ? kbd_index-1 : 0] == 0x01) {
                os_state=2; kbd_buffer[0]=0; kbd_index=0;
            }

            /* ---------- Drag finestre ---------- */
            static int drag_w=-1, drag_dx=0, drag_dy=0;
            Window* wins[2] = { &task_manager, &notepad_app };
            if (!mouse_click) { drag_w=-1; }
            if (mouse_click && drag_w==-1) {
                for (int i=1;i>=0;i--) {
                    Window* w=wins[i];
                    if (!w->minimized &&
                        mouse_x>w->x && mouse_x<w->x+w->w &&
                        mouse_y>w->y && mouse_y<w->y+28) {
                        drag_w=i; drag_dx=mouse_x-w->x; drag_dy=mouse_y-w->y; break;
                    }
                }
            }
            if (drag_w>=0) {
                wins[drag_w]->x=mouse_x-drag_dx;
                wins[drag_w]->y=mouse_y-drag_dy;
                if (wins[drag_w]->y<32) wins[drag_w]->y=32;
            }

            /* ---------- Click pulsanti finestra ---------- */
            static int prev_click=0;
            if (!prev_click && mouse_click) {
                for (int i=0;i<2;i++) {
                    Window* w=wins[i];
                    if (w->minimized) continue;
                    /* Chiudi */
                    if (mouse_x>w->x+4  && mouse_x<w->x+22 && mouse_y>w->y+2 && mouse_y<w->y+20) w->minimized=1;
                    /* Minimizza */
                    if (mouse_x>w->x+22 && mouse_x<w->x+40 && mouse_y>w->y+2 && mouse_y<w->y+20) w->minimized=1;
                }
                /* Click dock */
                int di=dock_icon_at(mouse_x, mouse_y, width, height);
                if (di==1) { task_manager.minimized^=1; }
                if (di==2) { notepad_app.minimized^=1;  }
            }
            prev_click=mouse_click;

            /* ---------- Topbar ---------- */
            draw_rect(backbuffer, 0, 0, width, 28, C_PANEL_BG, pitch);
            draw_hline(backbuffer, 0, 28, width, C_PANEL_BORDER, pitch);
            /* Logo */
            draw_string(backbuffer, "GafiteOS", 12, 10, C_ACCENT, 0xFFFFFFFF, pitch);
            /* Menu */
            draw_string(backbuffer, "File   Visualizza   Terminale   Aiuto",
                        100, 10, C_TEXT_MUT, 0xFFFFFFFF, pitch);
            /* Orologio */
            int h2, m2, s2; get_rtc_time(&h2, &m2, &s2);
            char ts[16] = { (char)((h2/10)+'0'), (char)((h2%10)+'0'), ':',
                            (char)((m2/10)+'0'), (char)((m2%10)+'0'), 0 };
            draw_string(backbuffer, ts, width-60, 10, C_TEXT_PRI, 0xFFFFFFFF, pitch);
            /* Username */
            draw_string(backbuffer, os_username, width-60-str_len(os_username)*8-12,
                        10, C_TEXT_MUT, 0xFFFFFFFF, pitch);

            /* ---------- Finestre ---------- */
            render_window(backbuffer, &task_manager, pitch);
            if (!task_manager.minimized) {
                int wx=task_manager.x, wy=task_manager.y;
                draw_string(backbuffer, "Kernel Heap:",
                            wx+20, wy+44, C_TEXT_MUT, 0xFFFFFFFF, pitch);
                /* Barra uso heap (fake 32%) */
                draw_rounded_rect(backbuffer, wx+20, wy+58, 270, 14, 4,
                                  C_PANEL_BORDER, pitch);
                draw_rounded_rect(backbuffer, wx+20, wy+58, 86, 14, 4,
                                  C_ACCENT, pitch);
                draw_string(backbuffer, "32%", wx+96, wy+60,
                            C_TEXT_PRI, 0xFFFFFFFF, pitch);

                draw_hline(backbuffer, wx+10, wy+82, task_manager.w-20,
                           C_PANEL_BORDER, pitch);

                draw_string(backbuffer, "Processi attivi:",
                            wx+20, wy+92, C_TEXT_MUT, 0xFFFFFFFF, pitch);
                draw_string(backbuffer, "kernel  [Ring 0]  running",
                            wx+20, wy+108, C_TEXT_PRI, 0xFFFFFFFF, pitch);

                draw_hline(backbuffer, wx+10, wy+128, task_manager.w-20,
                           C_PANEL_BORDER, pitch);
                draw_string(backbuffer, "Arch: x86  |  VBE Framebuffer",
                            wx+20, wy+136, C_TEXT_MUT, 0xFFFFFFFF, pitch);

                /* Riga risoluzione */
                char res_str[32];
                int ri=0;
                res_str[ri++]=(char)('0'+width/1000);
                res_str[ri++]=(char)('0'+(width/100)%10);
                res_str[ri++]=(char)('0'+(width/10)%10);
                res_str[ri++]=(char)('0'+width%10);
                res_str[ri++]='x';
                res_str[ri++]=(char)('0'+height/100);
                res_str[ri++]=(char)('0'+(height/10)%10);
                res_str[ri++]=(char)('0'+height%10);
                res_str[ri]=0;
                draw_string(backbuffer, res_str, wx+20, wy+152,
                            C_ACCENT, 0xFFFFFFFF, pitch);
            }

            render_window(backbuffer, &notepad_app, pitch);
            if (!notepad_app.minimized) {
                int wx=notepad_app.x, wy=notepad_app.y;
                /* Area testo */
                draw_rounded_rect(backbuffer, wx+10, wy+38,
                                  notepad_app.w-20, notepad_app.h-48,
                                  6, C_WIN_BG2, pitch);
                draw_string(backbuffer, kbd_buffer,
                            wx+18, wy+48, C_TEXT_PRI, 0xFFFFFFFF, pitch);
                /* Cursore testo */
                int tcx = wx+18+str_len(kbd_buffer)*8;
                draw_rect(backbuffer, tcx, wy+48, 2, 10, C_ACCENT, pitch);
            }

            /* ---------- Dock ---------- */
            int hov_icon=dock_icon_at(mouse_x, mouse_y, width, height);
            draw_dock(backbuffer, width, height, pitch, hov_icon);

        /* -------------------------------------------------------
         *  STATO 2 – Schermata di blocco
         * ------------------------------------------------------- */
        } else if (os_state == 2) {

            /* Blur simulato: scurisci */
            for (int i = 0; i < width * height; i++)
                backbuffer[i] = (backbuffer[i] >> 2) & 0x3F3F3F;

            int h2,m2,s2; get_rtc_time(&h2,&m2,&s2);

            /* Orario grande (2× scala via doppio carattere) */
            char big_time[16] = {
                (char)((h2/10)+'0'),(char)((h2%10)+'0'),':',
                (char)((m2/10)+'0'),(char)((m2%10)+'0'),':',
                (char)((s2/10)+'0'),(char)((s2%10)+'0'),0
            };
            /* Disegna 2× ingrandito */
            for (int ci=0; big_time[ci]; ci++) {
                int bx=width/2-36+ci*18, by=80;
                char tmp[2]={big_time[ci],0};
                for (int sy=0;sy<2;sy++) for (int sx=0;sx<2;sx++)
                    draw_string(backbuffer, tmp, bx+sx, by+sy*9,
                                C_TEXT_PRI, 0xFFFFFFFF, pitch);
            }
            draw_string_centered(backbuffer, "GafiteOS  –  Schermata di blocco",
                                 width/2, 116, C_TEXT_MUT, 0xFFFFFFFF, pitch);

            int bw=320, bh=170;
            int bx2=width/2-bw/2, by2=height/2-bh/2+40;

            draw_rect_alpha(backbuffer, bx2+6, by2+6, bw, bh, C_SHADOW, 140, pitch);
            draw_rounded_rect(backbuffer, bx2, by2, bw, bh, 12, C_WIN_BG, pitch);

            /* Avatar / nome */
            draw_rounded_rect(backbuffer, bx2+bw/2-20, by2+14, 40, 40, 20,
                              C_ACCENT2, pitch);
            draw_string_centered(backbuffer, os_username, bx2+bw/2,
                                 by2+62, C_TEXT_PRI, 0xFFFFFFFF, pitch);

            /* Input password */
            char stars[64]="";
            for(int z=0;z<kbd_index&&z<63;z++) stars[z]='*';
            render_input(backbuffer, bx2+20, by2+82, bw-40, 32,
                         stars, 1, pitch);

            /* Pulsante accedi */
            int abx=bx2+bw/2-55, aby=by2+128;
            int hov=(mouse_x>abx&&mouse_x<abx+110&&mouse_y>aby&&mouse_y<aby+32);
            render_button(backbuffer, abx, aby, 110, 32, "Accedi", hov, pitch);

            static int plk=0;
            if (!plk && mouse_click && hov) {
                if (str_eq(kbd_buffer, os_password)) {
                    os_state=1; kbd_buffer[0]=0; kbd_index=0;
                }
            }
            plk=mouse_click;
        }

        /* =========================================================
         *  Cursore freccia (sempre sopra tutto)
         * ========================================================= */
        draw_cursor(backbuffer, mouse_x, mouse_y, pitch);

        /* =========================================================
         *  Flip backbuffer → frontbuffer
         * ========================================================= */
        for (int i = 0; i < width * height; i++)
            frontbuffer[i] = backbuffer[i];
    }
}
