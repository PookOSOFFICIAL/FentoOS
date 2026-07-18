#include <types.h>
#include <hal.h>
#include <keyboard.h>
#include <sched.h>

#define KBD_DATA 0x60
#define KBD_STATUS 0x64
#define BUF_SIZE 256

static char kbuf[BUF_SIZE];
static volatile uint32_t khead, ktail;
static bool shift, ctrl, caps, extended;
static pid_t foreground;

static const char map_lower[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',0,
    '*',0,' ',0
};

static const char map_upper[128] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,'A','S','D','F','G','H','J','K','L',':','"','~',
    0,'|','Z','X','C','V','B','N','M','<','>','?',0,
    '*',0,' ',0
};

static int interrupt_foreground(void);

void keyboard_enqueue(char c) {
    if (c == 3 && interrupt_foreground()) return;
    uint32_t next = (khead + 1) % BUF_SIZE;
    if (next != ktail) {
        kbuf[khead] = c;
        khead = next;
    }
}

void keyboard_set_foreground(pid_t pid) {
    foreground = pid;
}

static int interrupt_foreground(void) {
    struct task *owner = sched_find(foreground);
    int killed = 0;
    if (!owner) return 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        struct task *t = sched_find_slot(i);
        if (!t || t->parent != owner || t->state == TASK_ZOMBIE) continue;
        t->exit_code = 130;
        t->state = TASK_ZOMBIE;
        sched_wakeup_parent(t);
        killed = 1;
    }
    return killed;
}

static void enqueue_escape(char code) {
    keyboard_enqueue(27);
    keyboard_enqueue('[');
    keyboard_enqueue(code);
}

static void kbd_handler(void *frame) {
    (void)frame;
    uint8_t sc = hal_inb(KBD_DATA);
    if (sc == 0xE0) {
        extended = true;
        return;
    }
    if (sc & 0x80) {
        uint8_t code = sc & 0x7F;
        if (code == 0x2A || code == 0x36) shift = false;
        else if (code == 0x1D) ctrl = false;
        extended = false;
        return;
    }
    if (extended) {
        extended = false;
        if (sc == 0x48) enqueue_escape('A');
        else if (sc == 0x50) enqueue_escape('B');
        else if (sc == 0x4D) enqueue_escape('C');
        else if (sc == 0x4B) enqueue_escape('D');
        else if (sc == 0x53) { enqueue_escape('3'); keyboard_enqueue('~'); }
        return;
    }
    if (sc == 0x2A || sc == 0x36) { shift = true; return; }
    if (sc == 0x1D) { ctrl = true; return; }
    if (sc == 0x3A) { caps = !caps; return; }
    if (sc >= 128) return;
    char c = (shift ^ caps) ? map_upper[sc] : map_lower[sc];
    if (!c) return;
    if (ctrl && c >= 'A' && c <= 'Z') c = c - 'A' + 1;
    else if (ctrl && c >= 'a' && c <= 'z') c = c - 'a' + 1;
    keyboard_enqueue(c);
}

void keyboard_init(void) {
    khead = ktail = 0;
    shift = ctrl = caps = extended = false;
    foreground = 0;
    hal_irq_register(1, kbd_handler);
    hal_irq_unmask(1);
    while (hal_inb(KBD_STATUS) & 1) hal_inb(KBD_DATA);
}

int keyboard_available(void) {
    return khead != ktail;
}

int keyboard_getchar(void) {
    if (khead == ktail) return -1;
    char c = kbuf[ktail];
    ktail = (ktail + 1) % BUF_SIZE;
    return (int)(uint8_t)c;
}
