# Linux System Monitor

A high-performance, modern Linux System Monitor and Process Management Suite written in C. Designed to interface directly with the Linux kernel virtual filesystems (`/proc` and `/sys`) and POSIX system APIs to provide real-time performance telemetry, interactive process control, and automated resource monitoring.

---

## 🚀 Key Features & OS Concepts (16 Total)

### Section A: System & Hardware Metrics
1. **System Information & OS Details:** OS release detection, kernel version, CPU architecture, hostname, online core count, frequency, user session (`uname()`, `/etc/os-release`, `/proc/cpuinfo`, `sysconf(_SC_NPROCESSORS_ONLN)`, `getpwuid()`).
2. **CPU Usage & Live Frequency Monitor:** Aggregate and per-core CPU utilization with non-blocking real-time frequency gauge (`/proc/stat` delta tick sampling, ANSI terminal escape sequences).
3. **Memory Utilization (RAM & Swap):** Physical RAM and Swap utilization, active/inactive cache breakdown, and colored status badges (`/proc/meminfo`).
4. **Disk & Multi-Mount Storage:** Multi-partition mounted filesystem discovery, total/used/free space, and root inode metrics (`/proc/mounts`, `statvfs()`).
5. **Running Processes & Process Tree:** Process state classification (R, S, D, T, Z) and hierarchical ASCII process tree (`opendir("/proc")`, `/proc/[pid]/stat`).
6. **System Uptime & Cumulative CPU Time:** Formatted uptime in days/hours/minutes/seconds and cumulative CPU active/idle time (`/proc/uptime`, `/proc/stat`).
7. **System Health Summary Dashboard:** Multi-factor composite health score across CPU, RAM, Disk, and Process concurrency.

### Section B: Process & Network Management
8. **Network Monitoring & Live Bandwidth:** Cumulative RX/TX data per interface with live real-time bandwidth speedometer in KB/s (`/proc/net/dev`).
9. **Top Resource-Consuming Processes:** Sorts Top 10 CPU & Top 10 RSS Memory processes with animated progress bars (two-pass CPU tick delta, `/proc/[pid]/statm`, `qsort()`).
10. **Process Search & Filter:** Search processes by PID or name substring with quick action prompt.
11. **Interactive Process Manager:** Signal management and scheduling priority control (`kill(pid, SIGTERM)`, `SIGKILL`, `SIGSTOP`, `SIGCONT`, `setpriority(PRIO_PROCESS, ...)`).

### Section C: Kernel Diagnostics & Telemetry
12. **System Load Averages & Context Switches:** 1, 5, and 15-minute load averages normalized per core, runnable thread pool counts, context switch counts, and fork rates (`/proc/loadavg`, `/proc/stat`).
13. **Kernel Modules Explorer (Loaded LKMs):** Linux Kernel Module (LKM) tracking, memory usage by kernel drivers, instance counts, and module search (`/proc/modules`).
14. **Resource Threshold Alerts:** Active telemetry checking for threshold breaches (>70% warning, >85% critical).
15. **Resource Usage Logging & History:** Timestamped snapshot logger persisting to `monitor_log.txt` with in-terminal history viewer.
16. **Exit Application:** Clean terminal exit.

---

## 🛠️ Build & Execution Instructions

### Method 1: Direct GCC Compilation (Recommended)
```bash
gcc -Wall -Wextra -O2 -std=gnu11 main.c -o monitor
./monitor
```

### Method 2: Using `make`
```bash
make
make run
```

To clean up binary and log files:
```bash
make clean
```

### Method 3: Quick Bash Script
```bash
chmod +x build.sh
./build.sh
```

> **Note:** To renice processes with negative priority values (higher priority) or manage root processes, run with `sudo`:
> ```bash
> sudo ./monitor
> ```
