#  User-Level Checkpoint & Restore System (Linux)

##  Overview
This project implements a **user-level checkpoint and restore mechanism** for Linux processes. It captures the execution state of a running process (memory, CPU registers, and file descriptors) and restores it later to simulate process recovery and fault tolerance.

---

##  Objectives
- Capture complete process state during execution
- Store checkpoint in a structured format
- Restore process state after termination
- Demonstrate fault-tolerant execution in Linux

---

## ⚙️Features

###  Process State Capture
- CPU Registers using `ptrace(PTRACE_GETREGS)`
- Memory regions using `/proc/<pid>/maps` and `/proc/<pid>/mem`
- File descriptors using `/proc/<pid>/fd`

---

###  Structured Checkpointing
- Stores:
  - Registers
  - Memory regions (with metadata)
  - File descriptors
- Uses binary format (`checkpoint_A.bin`, `checkpoint_B.bin`)

---
Exact instruction-level continuation is not always guaranteed due to:

Address Space Layout Randomization (ASLR)

Non-fixed memory mapping

Does not handle:

Multi-threaded processes

Pipes / sockets

Kernel-level resources
###  Rolling Checkpoint (A/B Strategy)
- Alternates between:
  - `checkpoint_A.bin`
  - `checkpoint_B.bin`
- Ensures:
  - Backup availability
  - Fault tolerance
  - Reduced data corruption risk

---

###  Process Restoration
- Creates a new process using `fork()`
- Reconstructs memory using `mmap()`
- Restores registers using `PTRACE_SETREGS`
- Reconnects file descriptors using `dup2()`

---

###  Automation
- `script.sh` enables periodic checkpointing
- Supports continuous monitoring and saving

---

##  Project Structure
- ├── attach.c # Checkpoint creation
- ├── restore.c # Process restoration
- ├── test.c # Sample process (demo)
- ├── script.sh # Automation script
- ├── README.md # Documentation

- Generated files
- ├── checkpoint_A.bin / checkpoint_B.bin
- ├── fds.txt
- ├── metadata.txt
- ├── log.txt

##  How to Run

### Demo WorkFlow

- gcc attach.c -o attach
- gcc restore.c -o restore
- gcc test.c -o test

- ./test

- ps aux | grep test

- sudo ./attach <pid>

- kill <pid>

- sudo ./restore

- tail -f log.txt

### Demo Explanation
- Process runs and updates a variable (x)

- Checkpoint is taken at runtime

- Process is terminated

- Restore reconstructs process state

- Execution behavior is validated using log output


### Limitations

- Exact instruction-level continuation is not always guaranteed due to:

- Address Space Layout Randomization (ASLR)

- Non-fixed memory mapping

#### Does not handle:

- Multi-threaded processes

- Pipes / sockets

- Kernel-level resources
