#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/user.h>
#include <fcntl.h>

int main() {

    FILE *in = fopen("checkpoint.bin", "rb");
    if (!in) {
        perror("checkpoint open failed");
        return 1;
    }

    // 🔹 Step 1: Read registers
    struct user_regs_struct regs;

    fread(&regs, sizeof(regs), 1, in);

    printf("Registers loaded from checkpoint.\n");

    // 🔹 Step 2: Create new process
    pid_t pid = fork();

    if (pid == 0) {
        // Child process

        // Allow tracing
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        execl("/bin/ls", "ls", NULL);  // dummy program
    }
    else {
        // Parent

        waitpid(pid, NULL, 0);

        printf("Child process created and stopped.\n");

        // 🔹 Step 3: Restore memory regions
        while (1) {

            unsigned long start;

            fread(&start, sizeof(start), 1, in);

            if (start == 0) break; // end marker

            unsigned long size;
            fread(&size, sizeof(size), 1, in);

            char *buffer = malloc(size);
            fread(buffer, 1, size, in);

            // Map memory at exact address
            void *addr = mmap((void *)start, size,
                              PROT_READ | PROT_WRITE | PROT_EXEC,
                              MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
                              -1, 0);

            if (addr == MAP_FAILED) {
                perror("mmap failed");
                free(buffer);
                continue;
            }

            // Copy memory
            memcpy(addr, buffer, size);

            printf("Restored region: %lx (%lu bytes)\n", start, size);

            free(buffer);
        }

        printf("Memory restored.\n");

        // 🔹 Step 4: Restore registers
        if (ptrace(PTRACE_SETREGS, pid, NULL, &regs) == -1) {
            perror("setregs failed");
            return 1;
        }

        printf("Registers restored.\n");

        // 🔹 Step 5: Resume execution
        ptrace(PTRACE_CONT, pid, NULL, NULL);

        printf("Process resumed from checkpoint.\n");
    }

    fclose(in);

    return 0;
}
