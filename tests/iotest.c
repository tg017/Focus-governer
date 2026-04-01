#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define FILE_NAME "stress_file.txt"
#define BUFFER_SIZE 4096

int main() {
    char buffer[BUFFER_SIZE];

    // Fill buffer with dummy data
    memset(buffer, 'A', BUFFER_SIZE);

    while (1) {
        // Delete file if it exists
        unlink(FILE_NAME);

        // Recreate file
        FILE *fp = fopen(FILE_NAME, "w+");
        if (fp == NULL) {
            perror("fopen");
            exit(EXIT_FAILURE);
        }

        // Heavy write loop
        for (int i = 0; i < 1000; i++) {
            fwrite(buffer, sizeof(char), BUFFER_SIZE, fp);
        }

        // Go back to beginning
        rewind(fp);

        // Heavy read loop
        for (int i = 0; i < 1000; i++) {
            fread(buffer, sizeof(char), BUFFER_SIZE, fp);
        }

        fclose(fp);
    }

    return 0;
}