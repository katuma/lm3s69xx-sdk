// test malloc, stdlib, stdio

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>

int main()
{
    write(0, "hello world\n", 12);
/*
    char *p = malloc(1024); int n;
    setbuf(stdout, NULL);
    while (1) {
        printf("Input buffer @ %p, type something ...\n", p);
        scanf("%s%n", p, &n);
        while (n) putchar(p[--n]);
        printf("\n");
    }*/
}
