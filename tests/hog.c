#include <pthread.h>

void* work(void* arg) {
    volatile int x = 0;

    while (1) {
        x++;   // prevents optimization
    }

    return NULL;
}

int main() {
    pthread_t t1, t2, t3;

    pthread_create(&t1, NULL, work, NULL);
    pthread_create(&t2, NULL, work, NULL);
    pthread_create(&t3, NULL, work, NULL);

    while (1) {}
}