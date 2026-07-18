#include <libc.h>

#define MAXARGS 32
#define MAXLINE 512
#define MAXCMDS 8

struct cmd {
    char *argv[MAXARGS];
    int argc;
    char *infile;
    char *outfile;
    int append;
};

static char cwd[256];

static int is_space(char c) {
    return c == ' ' || c == '\t';
}

static int tokenize(char *line, char **tokens, int max) {
    int n = 0;
    char *p = line;
    while (*p && n < max - 1) {
        while (is_space(*p)) p++;
        if (!*p) break;
        if (*p == '|' || *p == '<' || *p == '>') {
            if (*p == '>' && p[1] == '>') {
                tokens[n++] = ">>";
                *p = 0;
                p += 2;
            } else {
                static char ops[MAXCMDS * 4][2];
                static int opidx;
                char *op = ops[opidx++ % (MAXCMDS * 4)];
                op[0] = *p; op[1] = 0;
                tokens[n++] = op;
                p++;
            }
            continue;
        }
        tokens[n++] = p;
        while (*p && !is_space(*p) && *p != '|' && *p != '<' && *p != '>') p++;
        if (*p) { *p = 0; p++; }
    }
    tokens[n] = NULL;
    return n;
}

static int parse_pipeline(char **tokens, int ntok, struct cmd *cmds, int maxcmds) {
    int nc = 0;
    struct cmd *c = &cmds[0];
    memset(c, 0, sizeof(*c));
    nc = 1;
    for (int i = 0; i < ntok; i++) {
        char *t = tokens[i];
        if (strcmp(t, "|") == 0) {
            if (nc >= maxcmds) return -1;
            c = &cmds[nc];
            memset(c, 0, sizeof(*c));
            nc++;
        } else if (strcmp(t, "<") == 0) {
            if (++i >= ntok) return -1;
            c->infile = tokens[i];
        } else if (strcmp(t, ">") == 0) {
            if (++i >= ntok) return -1;
            c->outfile = tokens[i];
            c->append = 0;
        } else if (strcmp(t, ">>") == 0) {
            if (++i >= ntok) return -1;
            c->outfile = tokens[i];
            c->append = 1;
        } else {
            if (c->argc < MAXARGS - 1)
                c->argv[c->argc++] = t;
        }
    }
    for (int i = 0; i < nc; i++)
        cmds[i].argv[cmds[i].argc] = NULL;
    return nc;
}

static char *resolve(char *name, char *buf) {
    if (strchr(name, '/')) {
        strcpy(buf, name);
        return buf;
    }
    strcpy(buf, "/bin/");
    strcat(buf, name);
    return buf;
}

static int builtin(struct cmd *c) {
    if (c->argc == 0) return 1;
    char *cmd = c->argv[0];
    if (strcmp(cmd, "exit") == 0) {
        _exit(c->argc > 1 ? atoi(c->argv[1]) : 0);
    }
    if (strcmp(cmd, "cd") == 0) {
        const char *dir = c->argc > 1 ? c->argv[1] : "/";
        if (chdir(dir) < 0)
            fprintf(2, "cd: %s: no such directory\n", dir);
        return 1;
    }
    if (strcmp(cmd, "pwd") == 0) {
        if (getcwd(cwd, sizeof(cwd)))
            printf("%s\n", cwd);
        return 1;
    }
    return 0;
}

static void run_child(struct cmd *c) {
    if (c->infile) {
        int fd = open(c->infile, O_RDONLY, 0);
        if (fd < 0) { fprintf(2, "sh: cannot open %s\n", c->infile); _exit(1); }
        dup2(fd, 0);
        close(fd);
    }
    if (c->outfile) {
        int flags = O_WRONLY | O_CREAT | (c->append ? O_APPEND : O_TRUNC);
        int fd = open(c->outfile, flags, 0644);
        if (fd < 0) { fprintf(2, "sh: cannot open %s\n", c->outfile); _exit(1); }
        dup2(fd, 1);
        close(fd);
    }
    char path[256];
    char *envp[] = { "PATH=/bin", "HOME=/", NULL };
    resolve(c->argv[0], path);
    execve(path, c->argv, envp);
    fprintf(2, "sh: %s: command not found\n", c->argv[0]);
    _exit(127);
}

static void run_pipeline(struct cmd *cmds, int nc) {
    if (nc == 1 && builtin(&cmds[0])) return;

    int prev_read = -1;
    pid_t pids[MAXCMDS];

    for (int i = 0; i < nc; i++) {
        int pfd[2] = { -1, -1 };
        if (i < nc - 1) {
            if (pipe(pfd) < 0) { fprintf(2, "sh: pipe failed\n"); return; }
        }
        pid_t pid = fork();
        if (pid == 0) {
            if (prev_read >= 0) {
                dup2(prev_read, 0);
                close(prev_read);
            }
            if (pfd[1] >= 0) {
                dup2(pfd[1], 1);
                close(pfd[1]);
            }
            if (pfd[0] >= 0) close(pfd[0]);
            run_child(&cmds[i]);
            _exit(127);
        }
        pids[i] = pid;
        if (prev_read >= 0) close(prev_read);
        if (pfd[1] >= 0) close(pfd[1]);
        prev_read = pfd[0];
    }
    if (prev_read >= 0) close(prev_read);

    for (int i = 0; i < nc; i++) {
        int status;
        waitpid(pids[i], &status, 0);
    }
}

static int execute_line(char *line) {
    char *tokens[MAXARGS * 2];
    struct cmd cmds[MAXCMDS];
    while (is_space(*line)) line++;
    if (!*line || *line == '#') return 0;
    if (strcmp(line, "help") == 0) {
        printf("builtins: cd, pwd, exit, help\n");
        printf("tools: fcc fasm basic fento ls cat echo cp rm mkdir\n");
        printf("features: scripts, Ctrl-C, pipes, redirection, fork/exec\n");
        return 0;
    }
    int nt = tokenize(line, tokens, MAXARGS * 2);
    if (nt == 0) return 0;
    int nc = parse_pipeline(tokens, nt, cmds, MAXCMDS);
    if (nc <= 0) { fprintf(2, "sh: syntax error\n"); return 2; }
    run_pipeline(cmds, nc);
    return 0;
}

static int read_fd_line(int fd, char *line, int max) {
    int n = 0;
    int seen = 0;
    char c;
    while (n < max - 1) {
        int r = read(fd, &c, 1);
        if (r <= 0) return seen ? n : -1;
        seen = 1;
        if (c == '\r') continue;
        if (c == '\n') break;
        line[n++] = c;
    }
    line[n] = 0;
    return n;
}

int main(int argc, char **argv, char **envp) {
    (void)envp;
    char line[MAXLINE];
    ioctl(0, TTY_SET_FOREGROUND, (uint32_t)getpid());
    if (argc > 1) {
        int fd = open(argv[1], O_RDONLY, 0);
        if (fd < 0) { fprintf(2, "sh: cannot open %s\n", argv[1]); return 1; }
        for (;;) {
            int n = read_fd_line(fd, line, sizeof(line));
            if (n < 0) break;
            if (n > 0) execute_line(line);
        }
        close(fd);
        return 0;
    }
    printf("\nFentoOS development shell - type 'help'\n");
    for (;;) {
        if (!getcwd(cwd, sizeof(cwd))) strcpy(cwd, "?");
        printf("%s $ ", cwd);
        int n = getline(line, sizeof(line));
        if (n == 0) continue;
        if ((unsigned char)line[0] == 3) { printf("^C\n"); continue; }
        execute_line(line);
    }
}
