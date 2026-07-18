#include <libc.h>

#define CAP 32768
#define ROWS 23
#define COLS 80

static char text[CAP];
static int len, pos, top, dirty;
static const char *path;

static int line_start(int p) { while (p > 0 && text[p - 1] != '\n') p--; return p; }
static int line_end(int p) { while (p < len && text[p] != '\n') p++; return p; }
static int prev_line(int p) { p = line_start(p); return p ? line_start(p - 1) : 0; }
static int next_line(int p) { p = line_end(p); return p < len ? p + 1 : len; }
static int column(int p) { return p - line_start(p); }

static void locate(int p, int *row, int *col) {
    int r = 0, q = top;
    while (q < p && r < ROWS) { if (text[q++] == '\n') r++; }
    *row = r; *col = p - line_start(p); if (*col >= COLS) *col = COLS - 1;
}

static void adjust(void) {
    while (pos < top) top = prev_line(top);
    int r, c; locate(pos, &r, &c);
    while (r >= ROWS) { top = next_line(top); locate(pos, &r, &c); }
}

static void draw(void) {
    adjust();
    printf("\033[H");
    int p = top;
    for (int r = 0; r < ROWS; r++) {
        printf("\033[K"); int c = 0;
        while (p < len && text[p] != '\n' && c < COLS) { char ch = text[p++]; if (ch == '\t') { int n = 4 - c % 4; while (n-- && c < COLS) { putchar(' '); c++; } } else { putchar(ch); c++; } }
        while (p < len && text[p] != '\n') p++;
        if (p < len && text[p] == '\n') p++;
        if (r < ROWS - 1) putchar('\n');
    }
    printf("\033[24;1H\033[7m Fento  %s  %d bytes %s  Ctrl-S save  Ctrl-Q quit \033[K\033[0m", path, len, dirty ? "modified" : "saved");
    int row, col; locate(pos, &row, &col);
    printf("\033[%d;%dH", row + 1, col + 1);
}

static void insert(char c) {
    if (len >= CAP - 1) return;
    for (int i = len; i > pos; i--) text[i] = text[i - 1];
    text[pos++] = c; len++; dirty = 1;
}

static void backspace(void) {
    if (!pos) return; pos--; for (int i = pos; i < len - 1; i++) text[i] = text[i + 1]; len--; dirty = 1;
}

static int save(void) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644); if (fd < 0) return -1;
    int n = write(fd, text, len); close(fd); if (n == len) dirty = 0; return n == len ? 0 : -1;
}

int main(int argc, char **argv, char **envp) {
    (void)envp; path = argc > 1 ? argv[1] : "untitled.txt";
    int fd = open(path, O_RDONLY, 0); if (fd >= 0) { int n; while (len < CAP - 1 && (n = read(fd, text + len, CAP - 1 - len)) > 0) len += n; close(fd); }
    ioctl(0, TTY_SET_RAW, 1); ioctl(0, TTY_SET_ECHO, 0); printf("\033[2J\033[?25h");
    for (;;) {
        draw(); char c; if (read(0, &c, 1) <= 0) continue;
        if (c == 17) break;
        if (c == 19) { save(); continue; }
        if (c == 8 || c == 127) { backspace(); continue; }
        if (c == 27) {
            char a, b; if (read(0, &a, 1) <= 0 || a != '[' || read(0, &b, 1) <= 0) continue;
            if (b == 'D' && pos > 0) pos--;
            else if (b == 'C' && pos < len) pos++;
            else if (b == 'A') { int col = column(pos), p = prev_line(pos), e = line_end(p); pos = p + (col < e - p ? col : e - p); }
            else if (b == 'B') { int col = column(pos), p = next_line(pos), e = line_end(p); pos = p + (col < e - p ? col : e - p); }
            else if (b == '3') { char z; read(0, &z, 1); if (z == '~' && pos < len) { for (int i = pos; i < len - 1; i++) text[i] = text[i + 1]; len--; dirty = 1; } }
            continue;
        }
        if (c == '\r') c = '\n'; if ((unsigned char)c >= 32 || c == '\n' || c == '\t') insert(c);
    }
    ioctl(0, TTY_SET_RAW, 0); ioctl(0, TTY_SET_ECHO, 1); printf("\033[2J\033[H");
    return 0;
}
