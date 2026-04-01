#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N 2000   // matrix size (increase for more load)

// Allocate matrices globally to avoid stack overflow
double A[N][N];
double B[N][N];
double C[N][N];

void initialize() {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            A[i][j] = (double)rand() / RAND_MAX;
            B[i][j] = (double)rand() / RAND_MAX;
        }
    }
}

void multiply() {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            C[i][j] = 0.0;
            for (int k = 0; k < N; k++) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
}

int main() {
    srand(time(NULL));

    initialize();

    while (1) {
        multiply();
    }

    return 0;
}