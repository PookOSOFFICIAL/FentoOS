#include <libc.h>

static uint8_t out[32768]; static char src[32768]; static int n=0;
static void b(int v){out[n++]=v;} static void d(uint32_t v){b(v);b(v>>8);b(v>>16);b(v>>24);} static void p32(int p,uint32_t v){out[p]=v;out[p+1]=v>>8;out[p+2]=v>>16;out[p+3]=v>>24;}
static int reg(char *s){if(strncmp(s,"eax",3)==0)return 0;if(strncmp(s,"ecx",3)==0)return 1;if(strncmp(s,"edx",3)==0)return 2;if(strncmp(s,"ebx",3)==0)return 3;if(strncmp(s,"esp",3)==0)return 4;if(strncmp(s,"ebp",3)==0)return 5;if(strncmp(s,"esi",3)==0)return 6;if(strncmp(s,"edi",3)==0)return 7;return -1;}
static uint32_t num(char *s){while(*s==' '||*s=='\t'||*s==',')s++;uint32_t v=0;if(s[0]=='0'&&(s[1]=='x'||s[1]=='X')){s+=2;while(isdigit(*s)||(*s>='a'&&*s<='f')||(*s>='A'&&*s<='F')){int q=isdigit(*s)?*s-'0':tolower(*s)-'a'+10;v=v*16+q;s++;}}else v=atoi(s);return v;}
static void hdr(void){n=0;b(0x7f);b('E');b('L');b('F');b(1);b(1);b(1);for(int i=0;i<9;i++)b(0);b(2);b(0);b(3);b(0);d(1);d(0x08048100);d(52);d(0);d(0);b(52);b(0);b(32);b(0);b(1);b(0);for(int i=0;i<6;i++)b(0);d(1);d(0);d(0x08048000);d(0x08048000);d(0);d(0);d(7);d(0x1000);n=0x100;}
int main(int ac,char **av,char **ev){(void)ev;char *in=0,*dest="a.out";for(int i=1;i<ac;i++){if(strcmp(av[i],"-o")==0&&i+1<ac)dest=av[++i];else in=av[i];}if(!in){fprintf(2,"usage: fasm file.s [-o output]\n");return 1;}int fd=open(in,O_RDONLY,0);if(fd<0)return 1;int z=read(fd,src,sizeof(src)-1);close(fd);src[z>0?z:0]=0;hdr();char *q=src;int line=1;while(*q){while(*q==' '||*q=='\t')q++;char *e=q;while(*e&&*e!='\n'&&*e!=';')e++;char hold=*e;*e=0;if(*q&&*q!='#'&&*q!='.'){
if(strncmp(q,"mov ",4)==0){char *a=q+4;int r=reg(a);char *c=strchr(a,',');if(r<0||!c)goto bad;b(0xB8+r);d(num(c+1));}
else if(strncmp(q,"push ",5)==0){int r=reg(q+5);if(r>=0)b(0x50+r);else{b(0x68);d(num(q+5));}}
else if(strncmp(q,"pop ",4)==0){int r=reg(q+4);if(r<0)goto bad;b(0x58+r);}
else if(strncmp(q,"xor ",4)==0){int r=reg(q+4),s=reg(strchr(q,',')+1);b(0x31);b(0xC0+s*8+r);}
else if(strncmp(q,"add ",4)==0||strncmp(q,"sub ",4)==0){int sub=*q=='s',r=reg(q+4);char *c=strchr(q,',');if(r<0||!c)goto bad;b(0x81);b((sub?0xE8:0xC0)+r);d(num(c+1));}
else if(strncmp(q,"int ",4)==0){b(0xCD);b(num(q+4));}
else if(strcmp(q,"ret")==0)b(0xC3);else if(strcmp(q,"nop")==0)b(0x90);else if(strcmp(q,"leave")==0)b(0xC9);else if(strncmp(q,"db ",3)==0){char *x=q+3;while(*x){while(*x==' '||*x==',')x++;if(!*x)break;b(num(x));while(*x&&*x!=',')x++;}}else goto bad;}
*e=hold;if(hold=='\n'){line++;q=e+1;}else{while(*e&&*e!='\n')e++;if(*e){line++;q=e+1;}else q=e;}continue;bad:fprintf(2,"fasm: syntax error line %d: %s\n",line,q);return 1;}
p32(68,n);p32(72,n);fd=open(dest,O_WRONLY|O_CREAT|O_TRUNC,0755);if(fd<0)return 1;z=write(fd,out,n);close(fd);if(z!=n)return 1;printf("fasm: %s -> %s (%d bytes ELF32)\n",in,dest,n);return 0;}
