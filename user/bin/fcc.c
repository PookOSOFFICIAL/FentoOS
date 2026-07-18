#include <libc.h>

static uint8_t out[32768];
static char src[32768];
static int olen;
static char *xp;

static void emit8(int v) { out[olen++] = v; }
static void emit32(uint32_t v) { emit8(v); emit8(v >> 8); emit8(v >> 16); emit8(v >> 24); }
static void patch32(int p, uint32_t v) { out[p]=v; out[p+1]=v>>8; out[p+2]=v>>16; out[p+3]=v>>24; }
static void ws(void) { while (isspace(*xp)) xp++; }
static int value(void);
static int atom(void) { ws(); if (*xp=='(') { xp++; int v=value(); ws(); if(*xp==')')xp++; return v; } int s=1,v=0; if(*xp=='-'){s=-1;xp++;} while(isdigit(*xp))v=v*10+*xp++-'0'; return v*s; }
static int product(void) { int v=atom(); for(;;){ws(); if(*xp=='*'){xp++;v*=atom();}else if(*xp=='/'){xp++;int d=atom();v=d?v/d:0;}else if(*xp=='%'){xp++;int d=atom();v=d?v%d:0;}else return v;} }
static int value(void) { int v=product(); for(;;){ws();if(*xp=='+'){xp++;v+=product();}else if(*xp=='-'){xp++;v-=product();}else return v;} }

static void header(uint32_t size, uint32_t entry) {
    memset(out,0,256); olen=0;
    emit8(0x7f);emit8('E');emit8('L');emit8('F');emit8(1);emit8(1);emit8(1); for(int i=0;i<9;i++)emit8(0);
    emit8(2);emit8(0);emit8(3);emit8(0);emit32(1);emit32(entry);emit32(52);emit32(0);emit32(0);emit8(52);emit8(0);emit8(32);emit8(0);emit8(1);emit8(0);emit8(0);emit8(0);emit8(0);emit8(0);emit8(0);emit8(0);
    emit32(1);emit32(0);emit32(0x08048000);emit32(0x08048000);emit32(size);emit32(size);emit32(7);emit32(0x1000);
    olen=0x100;
}

int main(int argc,char **argv,char **envp) {
    (void)envp; const char *in=0,*dest="a.out";
    for(int i=1;i<argc;i++){if(strcmp(argv[i],"-o")==0&&i+1<argc)dest=argv[++i];else if(argv[i][0]!='-')in=argv[i];}
    if(!in){fprintf(2,"usage: fcc file.c [-o output]\n");return 1;}
    int fd=open(in,O_RDONLY,0);if(fd<0){fprintf(2,"fcc: cannot open %s\n",in);return 1;} int n=read(fd,src,sizeof(src)-1);close(fd);if(n<0)return 1;src[n]=0;
    header(0,0x08048100); int patches[64],plens[64],pc=0; char strings[8192];int sl=0;
    char *p=src;
    while(*p){
        if((strncmp(p,"puts",4)==0||strncmp(p,"printf",6)==0)&&strchr(p,'"')){
            int newline=strncmp(p,"puts",4)==0; p=strchr(p,'"')+1; int st=sl;
            while(*p&&*p!='"'&&sl<(int)sizeof(strings)-2){if(*p=='\\'&&p[1]){p++;if(*p=='n')strings[sl++]='\n';else if(*p=='t')strings[sl++]='\t';else strings[sl++]=*p++;}else strings[sl++]=*p++;}
            if(newline)strings[sl++]='\n'; emit8(0xB8);emit32(4);emit8(0xBB);emit32(1);emit8(0xB9);patches[pc]=olen;emit32(st);plens[pc++]=sl-st;emit8(0xBA);emit32(sl-st);emit8(0xCD);emit8(0x90);
        } else if(strncmp(p,"return",6)==0&&!isalnum(p[6])){xp=p+6;int v=value();emit8(0xB8);emit32(1);emit8(0xBB);emit32(v);emit8(0xCD);emit8(0x90);p=xp;}
        else p++;
    }
    if(olen==0x100||out[olen-2]!=0xCD){emit8(0xB8);emit32(1);emit8(0x31);emit8(0xDB);emit8(0xCD);emit8(0x90);}
    uint32_t base=0x08048000+olen;for(int i=0;i<pc;i++)patch32(patches[i],base+(uint32_t)(patches[i]>=0?0:0));
    for(int i=0;i<pc;i++){int off=patches[i];uint32_t idx=0;for(int j=0;j<i;j++)idx+=plens[j];patch32(off,base+idx);}
    memcpy(out+olen,strings,sl);olen+=sl;patch32(68,olen);patch32(72,olen);
    fd=open(dest,O_WRONLY|O_CREAT|O_TRUNC,0755);if(fd<0)return 1;if(write(fd,out,olen)!=olen){close(fd);return 1;}close(fd);printf("fcc: %s -> %s (%d bytes ELF32)\n",in,dest,olen);return 0;
}
