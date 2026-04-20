#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/user.h>
#include <fcntl.h>

typedef struct {
    unsigned long start;
    unsigned long size;
    char perms[5];
} region_meta;

// Convert permissions string → mmap protection
int get_prot(char *perms) {
    int prot = 0;
    if (perms[0] == 'r') prot |= PROT_READ;
    if (perms[1] == 'w') prot |= PROT_WRITE;
    if (perms[2] == 'x') prot |= PROT_EXEC;
    return prot;
}

int main() {

    FILE *in = fopen("checkpoint.bin", "rb");
    if (!in) {
        perror("Failed to open checkpoint.bin");
        return 1;
    }

    // 🔹 Step 1: Load registers
    struct user_regs_struct regs;
    if (fread(&regs, sizeof(regs), 1, in) != 1) {
        perror("Failed to read registers");
        fclose(in);
        return 1;
    }

    printf("Registers loaded.\n");

    // 🔹 Step 2: Create new process
    pid_t pid = fork();

    if (pid == 0) {
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        execl("/bin/true", "true", NULL);
        perror("execl failed");
        exit(1);
    }

    waitpid(pid, NULL, 0);
    printf("Child process created and stopped.\n");

    // 🔹 Step 3: Restore memory
    printf("Restoring memory...\n");

    while (1) {

        region_meta meta;

        if (fread(&meta, sizeof(meta), 1, in) != 1)
            break;

        if (meta.start == 0)
            break;

        char *buffer = malloc(meta.size);
        if (!buffer) continue;

        if (fread(buffer, 1, meta.size, in) != meta.size) {
            free(buffer);
            continue;
        }

        int prot = get_prot(meta.perms);

        // 🔥 SAFE mmap (NO MAP_FIXED)
        void *addr = mmap(NULL, meta.size,
                          PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS,
                          -1, 0);

        if (addr == MAP_FAILED) {
            perror("mmap failed");
            free(buffer);
            continue;
        }

        // Copy memory
        memcpy(addr, buffer, meta.size);

        // Set original permissions
        mprotect(addr, meta.size, prot);

        printf("Restored: %lx (%lu bytes)\n", meta.start, meta.size);

        free(buffer);
    }

    printf("Memory restored.\n");

    // 🔹 Step 4: Restore file descriptors
    FILE *fd_file = fopen("fds.txt", "r");

    if (fd_file) {
        int fd;
        char path[256];

        while (fscanf(fd_file, "%d %255s", &fd, path) == 2) {

            int newfd = open(path, O_RDWR);
            if (newfd < 0) continue;

            dup2(newfd, fd);
            close(newfd);
        }

        fclose(fd_file);
    }

    printf("File descriptors restored.\n");

    // 🔹 Step 5: Restore registers
    if (ptrace(PTRACE_SETREGS, pid, NULL, &regs) == -1) {
        perror("PTRACE_SETREGS failed");
        fclose(in);
        return 1;
    }

    printf("Registers restored.\n");

    // 🔹 Step 6: Resume execution
    if (ptrace(PTRACE_CONT, pid, NULL, NULL) == -1) {
        perror("PTRACE_CONT failed");
        fclose(in);
        return 1;
    }

    printf("Process resumed from checkpoint. PID: %d\n", pid);

    fclose(in);
    return 0;
}
