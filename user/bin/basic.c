#include <libc.h>

#define MAXLINES 256
#define LINELEN 128

struct line { int number; char text[LINELEN]; };
static struct line program[MAXLINES];
static int count;
static int vars[26];
static char *ep;

static void skip(void) { while (*ep == ' ' || *ep == '\t') ep++; }
static int expr(void);

static int factor(void) {
    skip();
    if (*ep == '(') { ep++; int v = expr(); skip(); if (*ep == ')') ep++; return v; }
    if (*ep == '-') { ep++; return -factor(); }
    if (isalpha(*ep)) { int n = toupper(*ep++) - 'A'; return vars[n]; }
    int v = 0;
    while (isdigit(*ep)) v = v * 10 + *ep++ - '0';
    return v;
}

static int term(void) {
    int v = factor();
    for (;;) {
        skip();
        if (*ep == '*') { ep++; v *= factor(); }
        else if (*ep == '/') { ep++; int d = factor(); v = d ? v / d : 0; }
        else if (*ep == '%') { ep++; int d = factor(); v = d ? v % d : 0; }
        else return v;
    }
}

static int expr(void) {
    int v = term();
    for (;;) {
        skip();
        if (*ep == '+') { ep++; v += term(); }
        else if (*ep == '-') { ep++; v -= term(); }
        else return v;
    }
}

static int find_line(int n) {
    for (int i = 0; i < count; i++) if (program[i].number == n) return i;
    return -1;
}

static void store(int n, const char *s) {
    int at = 0;
    while (at < count && program[at].number < n) at++;
    if (!*s) {
        if (at < count && program[at].number == n) {
            for (int i = at; i < count - 1; i++) program[i] = program[i + 1];
            count--;
        }
        return;
    }
    if (at < count && program[at].number == n) strcpy(program[at].text, s);
    else if (count < MAXLINES) {
        for (int i = count; i > at; i--) program[i] = program[i - 1];
        program[at].number = n;
        strncpy(program[at].text, s, LINELEN - 1);
        program[at].text[LINELEN - 1] = 0;
        count++;
    }
}

static int command(char *s, int *jump) {
    ep = s;
    skip();
    if (strncmp(ep, "REM", 3) == 0 || *ep == '\'') return 0;
    if (strncmp(ep, "PRINT", 5) == 0 || *ep == '?') {
        ep += *ep == '?' ? 1 : 5;
        skip();
        if (*ep == '"') {
            ep++;
            while (*ep && *ep != '"') putchar(*ep++);
        } else printf("%d", expr());
        printf("\n");
        return 0;
    }
    if (strncmp(ep, "INPUT", 5) == 0) {
        ep += 5; skip(); int v = toupper(*ep) - 'A'; char b[32]; printf("? "); getline(b, sizeof(b)); if (v >= 0 && v < 26) vars[v] = atoi(b); return 0;
    }
    if (strncmp(ep, "LET", 3) == 0) ep += 3;
    skip();
    if (isalpha(*ep) && (ep[1] == ' ' || ep[1] == '=')) {
        int v = toupper(*ep++) - 'A'; skip(); if (*ep == '=') ep++; vars[v] = expr(); return 0;
    }
    if (strncmp(ep, "GOTO", 4) == 0) { ep += 4; *jump = find_line(expr()); return *jump < 0 ? 1 : 2; }
    if (strncmp(ep, "IF", 2) == 0) {
        ep += 2; int a = expr(); skip(); int ok = 0;
        if (*ep == '=') { ep++; ok = a == expr(); }
        else if (*ep == '<') { ep++; ok = a < expr(); }
        else if (*ep == '>') { ep++; ok = a > expr(); }
        skip(); if (strncmp(ep, "THEN", 4) == 0) ep += 4;
        if (ok) { skip(); if (isdigit(*ep)) { *jump = find_line(expr()); return 2; } return command(ep, jump); }
        return 0;
    }
    if (strncmp(ep, "END", 3) == 0 || strncmp(ep, "STOP", 4) == 0) return 3;
    return 1;
}

static void run(void) {
    memset(vars, 0, sizeof(vars));
    int pc = 0;
    while (pc < count) {
        int j = -1;
        int r = command(program[pc].text, &j);
        if (r == 3) break;
        if (r == 2) pc = j;
        else { if (r == 1) printf("Error at %d\n", program[pc].number); pc++; }
    }
}

static void list(void) { for (int i = 0; i < count; i++) printf("%d %s\n", program[i].number, program[i].text); }

static int load(const char *path) {
    int fd = open(path, O_RDONLY, 0); if (fd < 0) return -1;
    char line[LINELEN]; int n = 0; char c;
    while (read(fd, &c, 1) > 0) {
        if (c == '\r') continue;
        if (c == '\n') { line[n] = 0; char *p = line; int no = atoi(p); while (isdigit(*p)) p++; while (*p == ' ') p++; if (no) store(no, p); n = 0; }
        else if (n < LINELEN - 1) line[n++] = c;
    }
    if (n) { line[n] = 0; char *p = line; int no = atoi(p); while (isdigit(*p)) p++; while (*p == ' ') p++; if (no) store(no, p); }
    close(fd); return 0;
}

static int save(const char *path) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644); if (fd < 0) return -1;
    for (int i = 0; i < count; i++) {
        char num[16]; int n = program[i].number, k = 0; char rev[16]; do { rev[k++] = '0' + n % 10; n /= 10; } while (n); int q = 0; while (k) num[q++] = rev[--k]; num[q++] = ' '; num[q] = 0;
        write(fd, num, q); write(fd, program[i].text, strlen(program[i].text)); write(fd, "\n", 1);
    }
    close(fd); return 0;
}

int main(int argc, char **argv, char **envp) {
    (void)envp;
    if (argc > 1) { if (load(argv[1]) < 0) return 1; run(); return 0; }
    printf("Fento BASIC 1.0\n");
    char line[LINELEN];
    for (;;) {
        printf("READY> "); if (!getline(line, sizeof(line))) continue;
        if (isdigit(line[0])) { char *p = line; int n = atoi(p); while (isdigit(*p)) p++; while (*p == ' ') p++; store(n, p); }
        else if (strcmp(line, "RUN") == 0) run();
        else if (strcmp(line, "LIST") == 0) list();
        else if (strcmp(line, "NEW") == 0) count = 0;
        else if (strncmp(line, "LOAD ", 5) == 0) { count = 0; if (load(line + 5) < 0) printf("Cannot load\n"); }
        else if (strncmp(line, "SAVE ", 5) == 0) { if (save(line + 5) < 0) printf("Cannot save\n"); }
        else if (strcmp(line, "BYE") == 0 || strcmp(line, "EXIT") == 0) break;
        else { int j = -1; if (command(line, &j) == 1) printf("Syntax error\n"); }
    }
    return 0;
}
