#include <types.h>
#include <hal.h>
#include <console.h>
#include <serial.h>

#define VGA_ADDR   0xB8000
#define VGA_COLS   80
#define VGA_ROWS   25

static volatile uint16_t *vga = (volatile uint16_t *)VGA_ADDR;
static uint8_t cur_row, cur_col;
static uint8_t color = 0x07;
static uint8_t esc_state;
static int esc_a, esc_b;
static bool esc_second;

static void update_cursor(void) {
    uint16_t pos = cur_row * VGA_COLS + cur_col;
    hal_outb(0x3D4, 0x0F);
    hal_outb(0x3D5, (uint8_t)(pos & 0xFF));
    hal_outb(0x3D4, 0x0E);
    hal_outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

static void scroll(void) {
    if (cur_row < VGA_ROWS) return;
    for (int r = 1; r < VGA_ROWS; r++)
        for (int c = 0; c < VGA_COLS; c++)
            vga[(r - 1) * VGA_COLS + c] = vga[r * VGA_COLS + c];
    for (int c = 0; c < VGA_COLS; c++)
        vga[(VGA_ROWS - 1) * VGA_COLS + c] = ((uint16_t)color << 8) | ' ';
    cur_row = VGA_ROWS - 1;
}

void console_set_color(uint8_t fg, uint8_t bg) {
    color = (bg << 4) | (fg & 0x0F);
}

void console_clear(void) {
    for (int i = 0; i < VGA_COLS * VGA_ROWS; i++)
        vga[i] = ((uint16_t)color << 8) | ' ';
    cur_row = cur_col = 0;
    update_cursor();
}

void console_init(void) {
    color = 0x07;
    console_clear();
}

void console_putc(char c) {
    serial_putc(c);
    if (!esc_state && c == 27) { esc_state = 1; esc_a = esc_b = 0; esc_second = false; return; }
    if (esc_state == 1) { if (c == '[') { esc_state = 2; return; } esc_state = 0; }
    if (esc_state == 2) {
        if (c == '?') return;
        if (c >= '0' && c <= '9') { if (esc_second) esc_b = esc_b * 10 + c - '0'; else esc_a = esc_a * 10 + c - '0'; return; }
        if (c == ';') { esc_second = true; return; }
        if (c == 'H' || c == 'f') { cur_row = esc_a > 0 ? esc_a - 1 : 0; cur_col = esc_b > 0 ? esc_b - 1 : 0; if (cur_row >= VGA_ROWS) cur_row = VGA_ROWS - 1; if (cur_col >= VGA_COLS) cur_col = VGA_COLS - 1; }
        else if (c == 'J' && esc_a == 2) console_clear();
        else if (c == 'K') for (int x = cur_col; x < VGA_COLS; x++) vga[cur_row * VGA_COLS + x] = ((uint16_t)color << 8) | ' ';
        else if (c == 'm') color = esc_a == 7 ? 0x70 : 0x07;
        esc_state = 0;
        update_cursor();
        return;
    }
    if (c == '\n') {
        cur_col = 0;
        cur_row++;
    } else if (c == '\r') {
        cur_col = 0;
    } else if (c == '\t') {
        cur_col = (cur_col + 8) & ~7;
    } else if (c == '\b') {
        if (cur_col > 0) {
            cur_col--;
            vga[cur_row * VGA_COLS + cur_col] = ((uint16_t)color << 8) | ' ';
        }
    } else {
        vga[cur_row * VGA_COLS + cur_col] = ((uint16_t)color << 8) | (uint8_t)c;
        cur_col++;
    }
    if (cur_col >= VGA_COLS) {
        cur_col = 0;
        cur_row++;
    }
    scroll();
    update_cursor();
}

void console_puts(const char *s) {
    while (*s) console_putc(*s++);
}

static void print_num(unsigned long value, int base, bool sign, int width, char pad) {
    char buf[32];
    int i = 0;
    bool neg = false;
    if (sign && (long)value < 0) {
        neg = true;
        value = (unsigned long)(-(long)value);
    }
    if (value == 0) buf[i++] = '0';
    while (value) {
        int d = value % base;
        buf[i++] = (d < 10) ? ('0' + d) : ('a' + d - 10);
        value /= base;
    }
    if (neg) buf[i++] = '-';
    while (i < width) buf[i++] = pad;
    while (i--) console_putc(buf[i]);
}

void console_printf(const char *fmt, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    while (*fmt) {
        if (*fmt != '%') {
            console_putc(*fmt++);
            continue;
        }
        fmt++;
        int width = 0;
        char pad = ' ';
        if (*fmt == '0') { pad = '0'; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') { width = width * 10 + (*fmt - '0'); fmt++; }
        switch (*fmt) {
            case 'd': case 'i':
                print_num((unsigned long)__builtin_va_arg(ap, int), 10, true, width, pad);
                break;
            case 'u':
                print_num((unsigned long)__builtin_va_arg(ap, unsigned int), 10, false, width, pad);
                break;
            case 'x':
                print_num((unsigned long)__builtin_va_arg(ap, unsigned int), 16, false, width, pad);
                break;
            case 'p':
                console_puts("0x");
                print_num((unsigned long)__builtin_va_arg(ap, void *), 16, false, 8, '0');
                break;
            case 'c':
                console_putc((char)__builtin_va_arg(ap, int));
                break;
            case 's': {
                const char *s = __builtin_va_arg(ap, const char *);
                if (!s) s = "(null)";
                console_puts(s);
                break;
            }
            case '%':
                console_putc('%');
                break;
            default:
                console_putc('%');
                console_putc(*fmt);
                break;
        }
        fmt++;
    }
    __builtin_va_end(ap);
}
