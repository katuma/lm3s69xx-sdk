// test malloc, stdlib, stdio

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>

int main()
{
    char *p = malloc(1024); int n;
    setbuf(stdout, NULL);
    while (1) {
        printf("Input buffer @ %p, type something, or 'exit' ...\n", p);
        scanf("%s%n", p, &n);
	if (!strcmp(p, "exit")) exit(0);
        while (n) putchar(p[--n]);
        printf("\n");
    }
}
