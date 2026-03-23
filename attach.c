#include <stdio.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/user.h>

int main(int argc, char *argv[]) {

    if (argc != 3) {
        printf("Usage: %s <pid> <hex_address>\n", argv[0]);
        return 1;
    }

    pid_t pid = atoi(argv[1]);
    unsigned long addr = strtoul(argv[2], NULL, 16);

    printf("Attaching to process %d...\n", pid);

    // Attach
    if (ptrace(PTRACE_ATTACH, pid, NULL, NULL) == -1) {
        perror("ptrace attach failed");
        return 1;
    }

    waitpid(pid, NULL, 0);
    printf("Process paused successfully.\n");

    // ======================
    // STEP 2: REGISTER READ
    // ======================
    struct user_regs_struct regs;

    if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) == -1) {
        perror("ptrace getregs failed");
        return 1;
    }

    printf("\n--- Register Values ---\n");
    printf("RIP: %llx\n", regs.rip);
    printf("RSP: %llx\n", regs.rsp);
    printf("RBP: %llx\n", regs.rbp);
    printf("------------------------\n");

    // ======================
    // STEP 3: MEMORY READ
    // ======================
    char mem_path[64];
    sprintf(mem_path, "/proc/%d/mem", pid);

    FILE *mem_file = fopen(mem_path, "r");
    if (!mem_file) {
        perror("fopen mem failed");
        ptrace(PTRACE_DETACH, pid, NULL, NULL);
        return 1;
    }

    // Move to address
    if (fseek(mem_file, addr, SEEK_SET) != 0) {
        perror("fseek failed");
        fclose(mem_file);
        ptrace(PTRACE_DETACH, pid, NULL, NULL);
        return 1;
    }

    char buffer[64];

    size_t bytes_read = fread(buffer, 1, sizeof(buffer), mem_file);
    if (bytes_read == 0) {
        perror("fread failed");
        fclose(mem_file);
        ptrace(PTRACE_DETACH, pid, NULL, NULL);
        return 1;
    }

    printf("\n--- Memory Data at %lx ---\n", addr);
    for (int i = 0; i < bytes_read; i++) {
        printf("%02x ", (unsigned char)buffer[i]);
    }
    printf("\n---------------------------\n");

    fclose(mem_file);

    sleep(3);

    // Detach
    printf("\nDetaching and resuming process...\n");

    if (ptrace(PTRACE_DETACH, pid, NULL, NULL) == -1) {
        perror("ptrace detach failed");
        return 1;
    }

    printf("Process resumed.\n");

    return 0;
}
