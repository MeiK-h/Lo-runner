#include </dev/random>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>

int main() {
    int a, b;
    while (1);
    scanf("%d %d", &a, &b);
    printf("%d\n", a + b);
    // open("asdf", O_CREAT);
    /*scanf("%d", &a);*/
    
    return 0;
}
