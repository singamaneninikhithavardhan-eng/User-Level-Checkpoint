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

    // 🔹 Step 2: Get registers
    struct user_regs_struct regs;

    if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) == -1) {
        perror("getregs failed");
        ptrace(PTRACE_DETACH, pid, NULL, NULL);
        return 1;
    }

    printf("Registers captured.\n");

    // 🔹 Step 3: Open files
    char maps_path[64], mem_path[64];

    sprintf(maps_path, "/proc/%d/maps", pid);
    sprintf(mem_path, "/proc/%d/mem", pid);

    FILE *maps = fopen(maps_path, "r");
    FILE *mem  = fopen(mem_path, "r");
    FILE *out  = fopen("checkpoint.bin", "wb");

    if (!maps || !mem || !out) {
        perror("file open failed");
        ptrace(PTRACE_DETACH, pid, NULL, NULL);
        return 1;
    }

    // 🔹 Step 4: WRITE REGISTERS FIRST
    fwrite(&regs, sizeof(regs), 1, out);

    printf("Registers written to checkpoint.\n");

    char line[256];

    // 🔹 Step 5: Loop through memory regions
    while (fgets(line, sizeof(line), maps)) {

        unsigned long start, end;
        char perms[5];

        sscanf(line, "%lx-%lx %4s", &start, &end, perms);

        // Only readable regions
        if (perms[0] != 'r') continue;

        unsigned long size = end - start;

        char *buffer = malloc(size);
        if (!buffer) continue;

        // Move to region
        if (fseek(mem, start, SEEK_SET) != 0) {
            free(buffer);
            continue;
        }

        size_t bytes = fread(buffer, 1, size, mem);
        if (bytes == 0) {
            free(buffer);
            continue;
        }

        // 🔹 WRITE METADATA (VERY IMPORTANT)
        fwrite(&start, sizeof(start), 1, out);
        fwrite(&size, sizeof(size), 1, out);

        // 🔹 WRITE MEMORY DATA
        fwrite(buffer, 1, bytes, out);

        printf("Saved region: %lx - %lx (%lu bytes)\n", start, end, size);

        free(buffer);
    }

    // 🔹 Step 6: END MARKER
    unsigned long end_marker = 0;
    fwrite(&end_marker, sizeof(end_marker), 1, out);

    printf("End marker written.\n");

    // 🔹 Close files
    fclose(maps);
    fclose(mem);
    fclose(out);

    printf("Checkpoint saved: checkpoint.bin\n");

    sleep(2);

    // 🔹 Detach
    ptrace(PTRACE_DETACH, pid, NULL, NULL);
    printf("Process resumed.\n");

    return 0;
}
