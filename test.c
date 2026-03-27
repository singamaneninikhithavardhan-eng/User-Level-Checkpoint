#include <stdio.h>
#include <unistd.h>

int main() {
    int x = 0;

    while (1) {
        printf("x = %d\nAaaadutham Biddaaaa....\n", x);
        x++;
        sleep(1);
    }

    return 0;
}
