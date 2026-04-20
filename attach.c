#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/user.h>
#include <dirent.h>
#include <limits.h>
#include <time.h>

typedef struct {
    unsigned long start;
    unsigned long size;
    char perms[5];
} region_meta;

int main(int argc, char *argv[]) {

    if (argc != 2) {
        printf("Usage: %s <pid>\n", argv[0]);
        return 1;
    }

    pid_t pid = atoi(argv[1]);

    // 🔹 Attach
    if (ptrace(PTRACE_ATTACH, pid, NULL, NULL) == -1) {
        perror("attach failed");
        return 1;
    }

    waitpid(pid, NULL, 0);
    printf("Process paused.\n");

    // 🔹 Get registers
    struct user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) == -1) {
        perror("getregs failed");
        return 1;
    }

    // 🔴 ROLLING CHECKPOINT LOGIC
    char filename[32] = "checkpoint_A.bin";

    FILE *meta_in = fopen("metadata.txt", "r");
    if (meta_in) {
        char line[128];
        while (fgets(line, sizeof(line), meta_in)) {
            if (strstr(line, "LATEST_CHECKPOINT=checkpoint_A.bin")) {
                strcpy(filename, "checkpoint_B.bin");
            } else if (strstr(line, "LATEST_CHECKPOINT=checkpoint_B.bin")) {
                strcpy(filename, "checkpoint_A.bin");
            }
        }
        fclose(meta_in);
    }

    printf("Saving checkpoint to %s\n", filename);

    FILE *out = fopen(filename, "wb");
    if (!out) {
        perror("checkpoint file open failed");
        return 1;
    }

    // 🔹 Write registers
    fwrite(&regs, sizeof(regs), 1, out);

    // 🔹 Open maps & mem
    char maps_path[64], mem_path[64];
    sprintf(maps_path, "/proc/%d/maps", pid);
    sprintf(mem_path, "/proc/%d/mem", pid);

    FILE *maps = fopen(maps_path, "r");
    FILE *mem  = fopen(mem_path, "r");

    if (!maps || !mem) {
        perror("maps/mem open failed");
        return 1;
    }

    char line[256];

    // 🔹 Memory dump
    while (fgets(line, sizeof(line), maps)) {

        unsigned long start, end;
        char perms[5];

        sscanf(line, "%lx-%lx %4s", &start, &end, perms);

        if (perms[0] != 'r') continue;

        unsigned long size = end - start;

        region_meta meta;
        meta.start = start;
        meta.size = size;
        strcpy(meta.perms, perms);

        char *buffer = malloc(size);
        if (!buffer) continue;

        if (fseek(mem, start, SEEK_SET) != 0) {
            free(buffer);
            continue;
        }

        size_t bytes = fread(buffer, 1, size, mem);
        if (bytes == 0) {
            free(buffer);
            continue;
        }

        fwrite(&meta, sizeof(meta), 1, out);
        fwrite(buffer, 1, bytes, out);

        printf("Saved: %lx (%lu bytes)\n", start, size);

        free(buffer);
    }

    // 🔹 End marker
    region_meta end = {0, 0, ""};
    fwrite(&end, sizeof(end), 1, out);

    fclose(maps);
    fclose(mem);
    fclose(out);

    printf("Memory checkpoint done.\n");

    // 🔴 STEP 6: FILE DESCRIPTOR CAPTURE
    DIR *dir;
    struct dirent *entry;

    char fd_path[64];
    sprintf(fd_path, "/proc/%d/fd", pid);

    dir = opendir(fd_path);
    FILE *fd_file = fopen("fds.txt", "w");

    if (dir && fd_file) {
        while ((entry = readdir(dir)) != NULL) {

            if (entry->d_name[0] == '.') continue;

            char link_path[128], target[PATH_MAX];

            sprintf(link_path, "%s/%s", fd_path, entry->d_name);

            ssize_t len = readlink(link_path, target, sizeof(target) - 1);

            if (len != -1) {
                target[len] = '\0';
                fprintf(fd_file, "%s %s\n", entry->d_name, target);
            }
        }

        closedir(dir);
        fclose(fd_file);
    }

    printf("File descriptors saved.\n");

    // 🔹 Update metadata
    FILE *meta = fopen("metadata.txt", "w");
    if (meta) {
        fprintf(meta, "LATEST_CHECKPOINT=%s\n", filename);
        fprintf(meta, "TIMESTAMP=%ld\n", time(NULL));
        fprintf(meta, "PID=%d\n", pid);
        fclose(meta);
    }

    // 🔹 Detach
    ptrace(PTRACE_DETACH, pid, NULL, NULL);

    printf("Checkpoint complete.\n");

    return 0;
}
