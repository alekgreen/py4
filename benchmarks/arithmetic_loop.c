#include <stdio.h>

#define ITERATIONS 5000000

static int work(int limit)
{
    int i = 0;
    int a = 1;
    int b = 2;
    int total = 0;

    while (i < limit) {
        a = a + b;
        while (a > 1000000) {
            a = a - 1000000;
        }

        b = b + total + 3;
        while (b > 1000000) {
            b = b - 1000000;
        }

        total = total + (a * 3) + (b * 2) + i;
        while (total > 1000000) {
            total = total - 1000000;
        }

        i = i + 1;
    }

    return total;
}

int main(void)
{
    printf("%d\n", work(ITERATIONS));
    return 0;
}
