#include <stdio.h>
#include <unistd.h>

int main() {
    int x = 0;

    FILE *f = fopen("log.txt", "a");

    while (1) {
        fprintf(f, "x = %d\n", x);
        fflush(f);   // VERY IMPORTANT
        x++;
        sleep(1);
    }

    return 0;
}
