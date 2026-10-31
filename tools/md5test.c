#include <stdio.h>
#include <string.h>
int b_fseek(void *fp, long off, int whence){return 0;}
void b_clearerr(void *fp){}
#include "backstab_shims.c"
int main(void){ b_MD5_CTX c; unsigned char d[16]; const char*s="The quick brown fox jumps over the lazy dog";
 b_MD5_Init(&c); b_MD5_Update(&c,s,strlen(s)); b_MD5_Final(d,&c);
 for(int i=0;i<16;i++)printf("%02x",d[i]); puts(""); return 0; }
