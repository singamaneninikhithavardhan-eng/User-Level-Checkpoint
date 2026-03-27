#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/user.h>

int main(int argc, char *argv[]) {

    if (argc != 2) {
        printf("Usage: %s <pid>\n", argv[0]);
        return 1;
    }

    pid_t pid = atoi(argv[1]);

    printf("Attaching to process %d...\n", pid);

    // 🔹 Step 1: Attach
    if (ptrace(PTRACE_ATTACH, pid, NULL, NULL) == -1) {
        perror("ptrace attach failed");
        return 1;
    }

    waitpid(pid, NULL, 0);
    printf("Process paused.\n");

    // 🔹 Step 2: Read registers (optional but useful)
    struct user_regs_struct regs;

    if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) == -1) {
        perror("getregs failed");
    } else {
        printf("RIP: %llx\n", regs.rip);
        printf("RSP: %llx\n", regs.rsp);
    }

    // 🔹 Step 3: Open maps file
    char maps_path[64];
    sprintf(maps_path, "/proc/%d/maps", pid);

    FILE *maps = fopen(maps_path, "r");
    if (!maps) {
        perror("maps open failed");
        return 1;
    }

    // 🔹 Step 4: Open mem file
    char mem_path[64];
    sprintf(mem_path, "/proc/%d/mem", pid);

    FILE *mem = fopen(mem_path, "r");
    if (!mem) {
        perror("mem open failed");
        return 1;
    }

    // 🔹 Step 5: Output checkpoint file
    FILE *out = fopen("checkpoint.bin", "wb");
    if (!out) {
        perror("output file failed");
        return 1;
    }

    printf("Dumping memory...\n");

    char line[256];

    // 🔹 Step 6: Loop through memory regions
    while (fgets(line, sizeof(line), maps)) {

        unsigned long start, end;
        char perms[5];

        sscanf(line, "%lx-%lx %4s", &start, &end, perms);

        // 🔹 Only readable regions
        if (perms[0] != 'r') continue;

        unsigned long size = end - start;

        char *buffer = malloc(size);
        if (!buffer) continue;

        // Move to memory region
        if (fseek(mem, start, SEEK_SET) != 0) {
            free(buffer);
            continue;
        }

        size_t bytes = fread(buffer, 1, size, mem);

        // Write to file
        fwrite(buffer, 1, bytes, out);

        printf("Dumped region: %lx - %lx (%lu bytes)\n", start, end, size);

        free(buffer);
    }

    fclose(maps);
    fclose(mem);
    fclose(out);

    printf("Memory dump saved to checkpoint.bin\n");

    sleep(2);

    // 🔹 Detach
    ptrace(PTRACE_DETACH, pid, NULL, NULL);
    printf("Process resumed.\n");

    return 0;
}
