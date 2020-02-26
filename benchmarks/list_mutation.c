#include <stdio.h>
#include <stdlib.h>

#define LIST_SIZE 2000
#define ROUNDS 1500

static int wrap_97(int value)
{
    while (value >= 97) {
        value -= 97;
    }
    return value;
}

static int wrap_5(int value)
{
    while (value >= 5) {
        value -= 5;
    }
    return value;
}

static int work(int size, int rounds)
{
    int *xs = malloc(sizeof(int) * (size_t)size);
    int total = 0;

    if (xs == NULL) {
        fprintf(stderr, "failed to allocate list benchmark buffer\n");
        exit(1);
    }

    for (int i = 0; i < size; i++) {
        xs[i] = wrap_97(i);
    }

    for (int r = 0; r < rounds; r++) {
        for (int j = 0; j < size; j++) {
            xs[j] = xs[j] + wrap_5(j + r);
            total += xs[j];
            while (total > 1000000) {
                total -= 1000000;
            }
        }
    }

    free(xs);
    return total;
}

int main(void)
{
    printf("%d\n", work(LIST_SIZE, ROUNDS));
    return 0;
}
