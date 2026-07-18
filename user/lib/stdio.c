#include <libc.h>
#include <stdarg.h>

void putchar(char c) {
    write(1, &c, 1);
}

void puts(const char *s) {
    write(1, s, strlen(s));
}

static void put_fd(int fd, const char *s) {
    write(fd, s, strlen(s));
}

static void itoa_u(uint32_t v, char *buf, int base, int upper) {
    const char *digl = "0123456789abcdef";
    const char *digu = "0123456789ABCDEF";
    const char *dig = upper ? digu : digl;
    char tmp[16];
    int i = 0;
    if (v == 0) tmp[i++] = '0';
    while (v) { tmp[i++] = dig[v % base]; v /= base; }
    int j = 0;
    while (i) buf[j++] = tmp[--i];
    buf[j] = 0;
}

static void itoa_s(int32_t v, char *buf) {
    if (v < 0) {
        buf[0] = '-';
        itoa_u((uint32_t)(-v), buf + 1, 10, 0);
    } else {
        itoa_u((uint32_t)v, buf, 10, 0);
    }
}

static void vformat(int fd, const char *fmt, va_list ap) {
    char nb[16];
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') { write(fd, p, 1); continue; }
        p++;
        switch (*p) {
            case 'd': case 'i':
                itoa_s(va_arg(ap, int), nb);
                put_fd(fd, nb);
                break;
            case 'u':
                itoa_u(va_arg(ap, uint32_t), nb, 10, 0);
                put_fd(fd, nb);
                break;
            case 'x':
                itoa_u(va_arg(ap, uint32_t), nb, 16, 0);
                put_fd(fd, nb);
                break;
            case 'X':
                itoa_u(va_arg(ap, uint32_t), nb, 16, 1);
                put_fd(fd, nb);
                break;
            case 'c': {
                char c = (char)va_arg(ap, int);
                write(fd, &c, 1);
                break;
            }
            case 's': {
                const char *s = va_arg(ap, const char *);
                if (!s) s = "(null)";
                put_fd(fd, s);
                break;
            }
            case '%':
                write(fd, "%", 1);
                break;
            case 0:
                return;
            default:
                write(fd, "%", 1);
                write(fd, p, 1);
                break;
        }
    }
}

void printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vformat(1, fmt, ap);
    va_end(ap);
}

void fprintf(int fd, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vformat(fd, fmt, ap);
    va_end(ap);
}

int getline(char *buf, int max) {
    int n = 0;
    while (n < max - 1) {
        char c;
        int r = read(0, &c, 1);
        if (r <= 0) break;
        if (c == '\n') break;
        if (c == '\b' || c == 127) {
            if (n > 0) n--;
            continue;
        }
        buf[n++] = c;
    }
    buf[n] = 0;
    return n;
}
