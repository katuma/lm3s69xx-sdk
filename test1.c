// test malloc, stdlib, stdio

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>

int a=10000,b,c=2800,d,e,f[2801],g;
int main()
{
    char *p = malloc(1024); int n;
    setbuf(stdout, NULL);
    while (1) {
        printf("Input buffer @ %p, type something ...\n", p);
        scanf("%s%n", p, &n);
        while (n) putchar(p[--n]);
        printf("\n");
    }
}
