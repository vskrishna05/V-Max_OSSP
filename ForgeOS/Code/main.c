#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <sys/resource.h>
#include <dirent.h>
#include <termios.h>
#include <sys/select.h>
#include <pwd.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <errno.h>

#define MAX_PROCESSES 4096
#define MAX_NAME 256
#define LOG_FILE "monitor_log.txt"

/* ANSI Colors */
#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_RED     "\033[1;31m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_BLUE    "\033[1;34m"
#define COLOR_CYAN    "\033[1;36m"
#define COLOR_WHITE   "\033[1;37m"
#define COLOR_GRAY    "\033[90m"

typedef struct {
    int pid;
    int ppid;
    char state;
    char name[MAX_NAME];
} ProcessInfo;

typedef struct {
    int pid;
    char name[MAX_NAME];
    double cpuUsage;
    double memoryUsage;
    unsigned long long memoryKB;
} ResourceProcess;

typedef struct {
    char name[64];
    unsigned long long rxBytes;
    unsigned long long txBytes;
} NetInterface;

typedef struct {
    char mountPoint[256];
    char device[128];
    char fsType[32];
    double totalGB;
    double usedGB;
    double freeGB;
    double usagePercent;
} DiskMount;

/* =========================================================
   FUNCTION PROTOTYPES
   ========================================================= */
void systemInformation(void);
void cpuUsage(void);
void memoryUsage(void);
void diskUsage(void);
void processMonitoring(void);
void systemUptime(void);
void systemHealthSummary(void);
void networkMonitoring(void);
void topResourceProcesses(void);
void processSearch(void);
void processManager(void);
void systemLoadAndScheduling(void);
void kernelModulesExplorer(void);
void resourceAlerts(void);
void resourceLogging(void);
void afterFeature(void);
int getCPUUsage(double *usage);
unsigned long long getTotalCPUTime(void);
int getMemoryUsage(double *usage);
int getDiskUsage(double *usage);
int getProcessCounts(int *total, int *running);
int readProcessMemory(int pid, unsigned long long *memoryKB);

/* Terminal & UI Management */
static struct termios orig_termios;
static int raw_mode_active = 0;

static void disableRawMode(void) {
    if (raw_mode_active) {
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
        raw_mode_active = 0;
    }
}

static void handle_sigint(int sig) {
    (void)sig;
    disableRawMode();
    printf("\n" COLOR_RESET "Process interrupted.\n");
    exit(0);
}

static void setupSignalHandler(void) {
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    atexit(disableRawMode);
}

static void enableRawMode(void) {
    if (raw_mode_active) return;
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) return;
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != -1) raw_mode_active = 1;
}

static int checkExitKey(int timeout_ms) {
    fd_set fds;
    struct timeval tv;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) <= 0) return 0;
    char ch;
    if (read(STDIN_FILENO, &ch, 1) <= 0) return 0;
    if (ch == 'q' || ch == 'Q' || ch == 'x' || ch == 'X' || ch == 3) return 1;
    if (ch == 27) {
        tv.tv_sec = 0; tv.tv_usec = 30000;
        FD_ZERO(&fds); FD_SET(STDIN_FILENO, &fds);
        if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0) {
            char seq[4] = {0};
            if (read(STDIN_FILENO, seq, sizeof(seq) - 1) > 0) {
                if (seq[0] == '[' && (seq[1] == 'F' || seq[1] == '4' || seq[1] == '1')) return 1;
            }
        } else {
            return 1;
        }
    }
    return 0;
}

static void clearScreen(void) { printf("\033[2J\033[H"); }
static void printHeader(const char *t) {
    printf("\n" COLOR_CYAN "========================================================\n" COLOR_RESET);
    printf("  " COLOR_BOLD COLOR_WHITE "%s\n" COLOR_RESET, t);
    printf(COLOR_CYAN "========================================================\n" COLOR_RESET);
}
static void printSection(const char *t) { printf("\n" COLOR_BLUE "--- %s ---\n" COLOR_RESET, t); }
static void printDivider(void) { printf(COLOR_GRAY "--------------------------------------------------------\n" COLOR_RESET); }

static void printProgressBar(const char *label, double percentage, int width) {
    int filled = (int)((percentage / 100.0) * width);
    if (filled > width) filled = width;
    if (filled < 0) filled = 0;
    const char *col = (percentage >= 85.0) ? COLOR_RED : ((percentage >= 70.0) ? COLOR_YELLOW : COLOR_GREEN);
    printf("%-16s [", label);
    printf("%s", col);
    for (int i = 0; i < filled; i++) printf("█");
    printf(COLOR_GRAY);
    for (int i = filled; i < width; i++) printf("░");
    printf(COLOR_RESET "] %s%5.1f%%" COLOR_RESET "\n", col, percentage);
}

static void printStatusBadge(double percentage) {
    if (percentage >= 85.0) printf("[" COLOR_RED "CRITICAL" COLOR_RESET "]");
    else if (percentage >= 70.0) printf("[" COLOR_YELLOW "WARNING" COLOR_RESET "]");
    else printf("[" COLOR_GREEN "NORMAL" COLOR_RESET "]");
}

static void formatBytes(unsigned long long bytes, char *buf, size_t sz) {
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    int i = 0;
    double d = (double)bytes;
    while (d >= 1024.0 && i < 4) { d /= 1024.0; i++; }
    snprintf(buf, sz, "%.2f %s", d, units[i]);
}

/* Unified /proc/[pid]/stat Parser to eliminate duplicate open/read blocks */
static int readProcStat(int pid, char *name, size_t nameSize, char *state, int *ppid, unsigned long long *cpuTime) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char line[2048];
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return 0;
    }
    fclose(f);

    char *lp = strchr(line, '(');
    char *rp = strrchr(line, ')');
    if (!lp || !rp || rp <= lp) return 0;

    if (name && nameSize > 0) {
        size_t len = (size_t)(rp - lp - 1);
        if (len >= nameSize) len = nameSize - 1;
        memcpy(name, lp + 1, len);
        name[len] = '\0';
    }

    char *after = rp + 2;
    char st = '?';
    int parent = 0;
    unsigned long long utime = 0, stime = 0;

    if (cpuTime) {
        long pgrp, session, tty, tpgid;
        unsigned long flags, minflt, cminflt, majflt, cmajflt;
        if (sscanf(after, "%c %d %ld %ld %ld %ld %lu %lu %lu %lu %lu %llu %llu",
                   &st, &parent, &pgrp, &session, &tty, &tpgid,
                   &flags, &minflt, &cminflt, &majflt, &cmajflt, &utime, &stime) >= 13) {
            *cpuTime = utime + stime;
        }
    } else {
        sscanf(after, "%c %d", &st, &parent);
    }

    if (state) *state = st;
    if (ppid) *ppid = parent;
    return 1;
}

/* 1. System Information */
void systemInformation(void) {
    struct utsname info;
    char distro[256] = "Linux", cpuModel[256] = "Generic CPU", cpuFreq[64] = "Unknown";
    long cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (uname(&info) != 0) return;

    FILE *osf = fopen("/etc/os-release", "r");
    if (osf) {
        char line[256];
        while (fgets(line, sizeof(line), osf)) {
            if (strncmp(line, "PRETTY_NAME=", 12) == 0) {
                char *s = line + 12;
                if (*s == '"') s++;
                char *e = strchr(s, '"');
                if (!e) e = strchr(s, '\n');
                if (e) *e = '\0';
                snprintf(distro, sizeof(distro), "%s", s);
                break;
            }
        }
        fclose(osf);
    }

    FILE *cpuf = fopen("/proc/cpuinfo", "r");
    if (cpuf) {
        char line[256];
        while (fgets(line, sizeof(line), cpuf)) {
            if (strncmp(line, "model name", 10) == 0 || strncmp(line, "Processor", 9) == 0) {
                char *c = strchr(line, ':');
                if (c) { c += 2; c[strcspn(c, "\r\n")] = '\0'; snprintf(cpuModel, sizeof(cpuModel), "%s", c); }
            } else if (strncmp(line, "cpu MHz", 7) == 0) {
                char *c = strchr(line, ':');
                double f;
                if (c && sscanf(c + 1, "%lf", &f) == 1) snprintf(cpuFreq, sizeof(cpuFreq), "%.2f MHz", f);
            }
        }
        fclose(cpuf);
    }

    struct passwd *pw = getpwuid(getuid());
    const char *username = pw ? pw->pw_name : getenv("USER");
    const char *home = pw ? pw->pw_dir : getenv("HOME");

    printHeader("SYSTEM INFORMATION");
    printSection("OS & KERNEL DETAILS");
    printf("%-20s : " COLOR_BOLD "%s" COLOR_RESET "\n", "Distribution", distro);
    printf("%-20s : %s (%s)\n", "Kernel", info.sysname, info.release);
    printf("%-20s : %s\n", "Architecture", info.machine);
    printf("%-20s : %s\n", "Hostname", info.nodename);

    printSection("PROCESSOR SPECIFICATIONS");
    printf("%-20s : %s\n", "CPU Model", cpuModel);
    printf("%-20s : %ld Cores (Online)\n", "Logical Cores", cores > 0 ? cores : 1);
    printf("%-20s : %s\n", "Frequency", cpuFreq);

    printSection("CURRENT USER SESSION");
    printf("%-20s : " COLOR_GREEN "%s" COLOR_RESET " (UID: %u)\n", "Username", username ? username : "Unknown", (unsigned)getuid());
    printf("%-20s : %s\n", "Home Directory", home ? home : "Unknown");
    printDivider();
}

/* 2. CPU Usage & Helper */
int getCPUUsage(double *usage) {
    FILE *f1 = fopen("/proc/stat", "r");
    if (!f1) return 0;
    unsigned long long u1, n1, s1, i1, io1, ir1, sir1, st1;
    char line[256];
    if (!fgets(line, sizeof(line), f1) ||
        sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
               &u1, &n1, &s1, &i1, &io1, &ir1, &sir1, &st1) != 8) {
        fclose(f1); return 0;
    }
    fclose(f1);

    usleep(300000); /* 300ms responsive sampling */

    FILE *f2 = fopen("/proc/stat", "r");
    if (!f2) return 0;
    unsigned long long u2, n2, s2, i2, io2, ir2, sir2, st2;
    if (!fgets(line, sizeof(line), f2) ||
        sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
               &u2, &n2, &s2, &i2, &io2, &ir2, &sir2, &st2) != 8) {
        fclose(f2); return 0;
    }
    fclose(f2);

    unsigned long long t1 = u1 + n1 + s1 + i1 + io1 + ir1 + sir1 + st1;
    unsigned long long t2 = u2 + n2 + s2 + i2 + io2 + ir2 + sir2 + st2;
    unsigned long long idle1 = i1 + io1, idle2 = i2 + io2;
    unsigned long long dt = t2 - t1, didle = idle2 - idle1;

    *usage = (dt > 0) ? (100.0 * (double)(dt - didle) / (double)dt) : 0.0;
    return 1;
}

unsigned long long getTotalCPUTime(void) {
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return 0;
    char line[256];
    if (!fgets(line, sizeof(line), f)) { fclose(f); return 0; }
    fclose(f);
    unsigned long long u, n, s, i, io, ir, sir, st;
    if (sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
               &u, &n, &s, &i, &io, &ir, &sir, &st) != 8) return 0;
    return u + n + s + i + io + ir + sir + st;
}

void cpuUsage(void) {
    double overall = 0.0;
    printf(COLOR_CYAN "\nMeasuring CPU usage...\n" COLOR_RESET);
    if (!getCPUUsage(&overall)) return;

    printHeader("CPU USAGE & REAL-TIME MONITOR");
    printProgressBar("Overall CPU", overall, 24);
    printStatusBadge(overall);
    printf("\n");

    printf("\n" COLOR_BOLD "Options:" COLOR_RESET "\n");
    printf("  0. Start Real-Time Live CPU Monitor\n");
    printf("  1. Return to Main Menu\n");
    printf("Enter choice: ");

    int c;
    if (scanf("%d", &c) != 1 || c != 0) { while (getchar() != '\n'); return; }

    printf("\nLive monitoring active. Press " COLOR_YELLOW "'q', ESC, or End" COLOR_RESET " to stop.\n");
    sleep(1);
    enableRawMode();
    while (1) {
        if (checkExitKey(400)) break;
        double cur = 0.0;
        if (getCPUUsage(&cur)) {
            printf("\r\033[K" COLOR_CYAN "[LIVE] " COLOR_RESET);
            printProgressBar("CPU Usage", cur, 20);
            fflush(stdout);
        }
    }
    disableRawMode();
    printf("\n" COLOR_GREEN "Real-time monitoring stopped." COLOR_RESET "\n");
}

/* 3. Memory Usage */
int getMemoryUsage(double *usage) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return 0;
    char line[256];
    long totalKB = 0, availKB = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "MemTotal:", 9) == 0) sscanf(line, "MemTotal: %ld kB", &totalKB);
        else if (strncmp(line, "MemAvailable:", 13) == 0) sscanf(line, "MemAvailable: %ld kB", &availKB);
    }
    fclose(f);
    if (totalKB <= 0) return 0;
    *usage = ((double)(totalKB - availKB) / (double)totalKB) * 100.0;
    return 1;
}

void memoryUsage(void) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return;
    long totalKB = 0, availKB = 0, cachedKB = 0, buffersKB = 0;
    long shmemKB = 0, activeKB = 0, inactiveKB = 0, swapTotalKB = 0, swapFreeKB = 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "MemTotal:", 9) == 0) sscanf(line, "MemTotal: %ld kB", &totalKB);
        else if (strncmp(line, "MemAvailable:", 13) == 0) sscanf(line, "MemAvailable: %ld kB", &availKB);
        else if (strncmp(line, "Cached:", 7) == 0) sscanf(line, "Cached: %ld kB", &cachedKB);
        else if (strncmp(line, "Buffers:", 8) == 0) sscanf(line, "Buffers: %ld kB", &buffersKB);
        else if (strncmp(line, "Shmem:", 6) == 0) sscanf(line, "Shmem: %ld kB", &shmemKB);
        else if (strncmp(line, "Active:", 7) == 0) sscanf(line, "Active: %ld kB", &activeKB);
        else if (strncmp(line, "Inactive:", 9) == 0) sscanf(line, "Inactive: %ld kB", &inactiveKB);
        else if (strncmp(line, "SwapTotal:", 10) == 0) sscanf(line, "SwapTotal: %ld kB", &swapTotalKB);
        else if (strncmp(line, "SwapFree:", 9) == 0) sscanf(line, "SwapFree: %ld kB", &swapFreeKB);
    }
    fclose(f);
    if (totalKB <= 0) return;

    long usedKB = totalKB - availKB;
    double memPct = ((double)usedKB / (double)totalKB) * 100.0;
    long usedSwapKB = swapTotalKB - swapFreeKB;
    double swapPct = (swapTotalKB > 0) ? (((double)usedSwapKB / (double)swapTotalKB) * 100.0) : 0.0;

    printHeader("MEMORY ALLOCATION & USAGE");
    printSection("RAM UTILIZATION");
    printProgressBar("Physical RAM", memPct, 24);
    printf("%-18s : %.2f GB\n", "Total Memory", totalKB / (1024.0 * 1024.0));
    printf("%-18s : %.2f GB\n", "Used Memory", usedKB / (1024.0 * 1024.0));
    printf("%-18s : %.2f GB\n", "Available Memory", availKB / (1024.0 * 1024.0));
    printf("%-18s : ", "Health Status");
    printStatusBadge(memPct);
    printf("\n");

    printSection("MEMORY BREAKDOWN");
    printf("%-18s : %.2f GB\n", "Cached", cachedKB / (1024.0 * 1024.0));
    printf("%-18s : %.2f GB\n", "Buffers", buffersKB / (1024.0 * 1024.0));
    printf("%-18s : %.2f GB\n", "Shared", shmemKB / (1024.0 * 1024.0));

    printSection("SWAP MEMORY");
    if (swapTotalKB > 0) {
        printProgressBar("Swap Memory", swapPct, 24);
        printf("%-18s : %.2f GB (Used: %.2f GB)\n", "Total Swap", swapTotalKB / (1024.0 * 1024.0), usedSwapKB / (1024.0 * 1024.0));
    } else {
        printf("No active swap space configured.\n");
    }
    printDivider();
}

/* 4. Disk Usage */
static void setDiskMount(DiskMount *m, const char *dev, const char *mnt, const char *fs, struct statvfs *st) {
    snprintf(m->device, sizeof(m->device), "%s", dev);
    snprintf(m->mountPoint, sizeof(m->mountPoint), "%s", mnt);
    snprintf(m->fsType, sizeof(m->fsType), "%s", fs);
    unsigned long long total = (unsigned long long)st->f_blocks * st->f_frsize;
    unsigned long long freeB = (unsigned long long)st->f_bavail * st->f_frsize;
    unsigned long long usedB = total > freeB ? total - freeB : 0;
    m->totalGB = (double)total / (1024.0 * 1024.0 * 1024.0);
    m->usedGB = (double)usedB / (1024.0 * 1024.0 * 1024.0);
    m->freeGB = (double)freeB / (1024.0 * 1024.0 * 1024.0);
    m->usagePercent = total > 0 ? (100.0 * (double)usedB / (double)total) : 0.0;
}

int getDiskUsage(double *usage) {
    struct statvfs st;
    if (statvfs("/", &st) != 0 || st.f_blocks == 0) return 0;
    unsigned long long total = (unsigned long long)st.f_blocks * st.f_frsize;
    unsigned long long freeSpace = (unsigned long long)st.f_bfree * st.f_frsize;
    *usage = ((double)(total - freeSpace) / (double)total) * 100.0;
    return 1;
}

void diskUsage(void) {
    printHeader("STORAGE & FILESYSTEM USAGE");
    FILE *mf = fopen("/proc/mounts", "r");
    DiskMount mounts[32];
    int count = 0;

    if (mf) {
        char line[512];
        while (fgets(line, sizeof(line), mf) && count < 32) {
            char dev[128], mnt[256], fstype[32];
            if (sscanf(line, "%127s %255s %31s", dev, mnt, fstype) == 3) {
                if (strncmp(dev, "/dev/", 5) == 0 || strcmp(mnt, "/") == 0) {
                    struct statvfs st;
                    if (statvfs(mnt, &st) == 0 && st.f_blocks > 0) {
                        int dup = 0;
                        for (int i = 0; i < count; i++) {
                            if (strcmp(mounts[i].mountPoint, mnt) == 0) { dup = 1; break; }
                        }
                        if (dup) continue;
                        setDiskMount(&mounts[count++], dev, mnt, fstype, &st);
                    }
                }
            }
        }
        fclose(mf);
    }

    if (count == 0) {
        struct statvfs st;
        if (statvfs("/", &st) == 0 && st.f_blocks > 0) {
            setDiskMount(&mounts[0], "/dev/root", "/", "ext4", &st);
            count = 1;
        }
    }

    printSection("MOUNTED PARTITIONS");
    printf("%-14s %-16s %-8s %-9s %-9s %-6s\n", "DEVICE", "MOUNT", "FSTYPE", "TOTAL", "FREE", "USAGE");
    printDivider();
    for (int i = 0; i < count; i++) {
        printf("%-14s %-16s %-8s %7.2fGB %7.2fGB %5.1f%%\n",
               mounts[i].device, mounts[i].mountPoint, mounts[i].fsType,
               mounts[i].totalGB, mounts[i].freeGB, mounts[i].usagePercent);
        printProgressBar(mounts[i].mountPoint, mounts[i].usagePercent, 20);
    }
    printDivider();
}

/* 5. Process Monitoring & Process Tree */
int getProcessCounts(int *total, int *running) {
    DIR *dir = opendir("/proc");
    if (!dir) return 0;
    *total = 0; *running = 0;
    struct dirent *e;
    while ((e = readdir(dir)) != NULL) {
        if (!isdigit((unsigned char)e->d_name[0])) continue;
        (*total)++;
        char st = '?';
        if (readProcStat(atoi(e->d_name), NULL, 0, &st, NULL, NULL) && st == 'R') {
            (*running)++;
        }
    }
    closedir(dir);
    return 1;
}

static void printTreeRecursive(ProcessInfo *processes, int count, int parentPid, int level, int maxDepth) {
    if (level > maxDepth) return;
    for (int i = 0; i < count; i++) {
        if (processes[i].ppid == parentPid) {
            for (int j = 0; j < level; j++) printf("  ");
            if (level > 0) printf(COLOR_CYAN "└─ " COLOR_RESET);
            printf("%s " COLOR_GRAY "(PID: %d)" COLOR_RESET "\n", processes[i].name, processes[i].pid);
            printTreeRecursive(processes, count, processes[i].pid, level + 1, maxDepth);
        }
    }
}

void processMonitoring(void) {
    ProcessInfo *procs = malloc(sizeof(ProcessInfo) * MAX_PROCESSES);
    if (!procs) return;

    DIR *dir = opendir("/proc");
    if (!dir) { free(procs); return; }

    int count = 0, rCount = 0, sCount = 0, tCount = 0, zCount = 0;
    struct dirent *e;

    while ((e = readdir(dir)) != NULL && count < MAX_PROCESSES) {
        if (!isdigit((unsigned char)e->d_name[0])) continue;
        int pid = atoi(e->d_name);
        char st = '?';
        int ppid = 0;
        if (readProcStat(pid, procs[count].name, MAX_NAME, &st, &ppid, NULL)) {
            procs[count].pid = pid;
            procs[count].ppid = ppid;
            procs[count].state = st;
            if (st == 'R') rCount++;
            else if (st == 'S' || st == 'D') sCount++;
            else if (st == 'T') tCount++;
            else if (st == 'Z') zCount++;
            count++;
        }
    }
    closedir(dir);

    printHeader("RUNNING PROCESSES & MONITORING");
    printf(COLOR_BOLD "Process Counts:" COLOR_RESET " Total: %d | " COLOR_GREEN "Running: %d" COLOR_RESET " | Sleeping: %d | Stopped: %d | " COLOR_RED "Zombie: %d" COLOR_RESET "\n\n",
           count, rCount, sCount, tCount, zCount);

    printf("%-8s %-8s %-7s %-28s\n", "PID", "PPID", "STATE", "COMMAND");
    printDivider();
    int limit = (count < 30) ? count : 30;
    for (int i = 0; i < limit; i++) {
        const char *cs = (procs[i].state == 'R') ? COLOR_GREEN : ((procs[i].state == 'Z') ? COLOR_RED : COLOR_WHITE);
        printf("%-8d %-8d %s%-7c" COLOR_RESET " %-28s\n", procs[i].pid, procs[i].ppid, cs, procs[i].state, procs[i].name);
    }
    if (count > 30) printf(COLOR_GRAY "... and %d more processes.\n" COLOR_RESET, count - 30);

    printf("\n" COLOR_BOLD "Submenu Options:" COLOR_RESET "\n");
    printf("  1. View Hierarchical Process Tree\n");
    printf("  2. Open Process Manager (Kill / Pause / Renice)\n");
    printf("  3. Return to Main Menu\n");
    printf("Enter choice: ");

    int subchoice;
    if (scanf("%d", &subchoice) == 1) {
        if (subchoice == 1) {
            printSection("PROCESS HIERARCHY TREE (Top-Level)");
            printTreeRecursive(procs, count, 0, 0, 4);
        } else if (subchoice == 2) {
            processManager();
        }
    } else {
        while (getchar() != '\n');
    }
    free(procs);
}

/* 6. System Uptime */
void systemUptime(void) {
    FILE *f = fopen("/proc/uptime", "r");
    if (!f) return;
    double upSec = 0.0, idleSec = 0.0;
    if (fscanf(f, "%lf %lf", &upSec, &idleSec) < 1) { fclose(f); return; }
    fclose(f);

    long t = (long)upSec;
    long d = t / 86400, h = (t % 86400) / 3600, m = (t % 3600) / 60, s = t % 60;

    printHeader("SYSTEM UPTIME & SYSTEM HEALTH");
    printf("%-20s : " COLOR_BOLD COLOR_GREEN "%ldd %ldh %ldm %lds" COLOR_RESET "\n", "Formatted Uptime", d, h, m, s);
    printf("%-20s : %.2f Hours (%.0f Total Seconds)\n", "Total Uptime", upSec / 3600.0, upSec);
    printf("%-20s : %.2f Hours\n", "Cumulative Idle", idleSec / 3600.0);
    printDivider();
}

/* 7. System Health Summary */
void systemHealthSummary(void) {
    double cpu = 0.0, mem = 0.0, disk = 0.0;
    int totalP = 0, runP = 0;
    printf(COLOR_CYAN "\nEvaluating system health indicators...\n" COLOR_RESET);
    int cOK = getCPUUsage(&cpu), mOK = getMemoryUsage(&mem), dOK = getDiskUsage(&disk);
    int pOK = getProcessCounts(&totalP, &runP);

    printHeader("SYSTEM HEALTH SUMMARY DASHBOARD");
    printSection("RESOURCE UTILIZATION GAUGES");
    if (cOK) printProgressBar("CPU Load", cpu, 22);
    if (mOK) printProgressBar("RAM Memory", mem, 22);
    if (dOK) printProgressBar("Disk Root (/)", disk, 22);
    if (pOK) printf("\n%-18s : %d total | " COLOR_GREEN "%d running" COLOR_RESET "\n", "Process State", totalP, runP);

    double worst = cpu > mem ? (cpu > disk ? cpu : disk) : (mem > disk ? mem : disk);
    printf("\nComposite Status : ");
    printStatusBadge(worst);
    printf("\n");
    printDivider();
}

/* 8. Network Monitoring */
static int readNetDev(NetInterface *ifaces, int maxIfaces) {
    FILE *f = fopen("/proc/net/dev", "r");
    if (!f) return 0;
    char line[512];
    int count = 0;
    while (fgets(line, sizeof(line), f) && count < maxIfaces) {
        char *col = strchr(line, ':');
        if (!col) continue;
        *col = '\0';
        char ifname[64];
        if (sscanf(line, "%63s", ifname) != 1) continue;
        unsigned long long rx = 0, tx = 0, dummy[7];
        if (sscanf(col + 1, "%llu %llu %llu %llu %llu %llu %llu %llu %llu",
                   &rx, &dummy[0], &dummy[1], &dummy[2], &dummy[3], &dummy[4], &dummy[5], &dummy[6], &tx) == 9) {
            snprintf(ifaces[count].name, sizeof(ifaces[count].name), "%s", ifname);
            ifaces[count].rxBytes = rx;
            ifaces[count].txBytes = tx;
            count++;
        }
    }
    fclose(f);
    return count;
}

void networkMonitoring(void) {
    NetInterface ifaces[16];
    int count = readNetDev(ifaces, 16);
    if (count == 0) return;

    printHeader("NETWORK INTERFACE MONITOR");
    printf("%-18s %-18s %-18s\n", "INTERFACE", "RECEIVED (RX)", "TRANSMITTED (TX)");
    printDivider();
    for (int i = 0; i < count; i++) {
        char rxS[32], txS[32];
        formatBytes(ifaces[i].rxBytes, rxS, sizeof(rxS));
        formatBytes(ifaces[i].txBytes, txS, sizeof(txS));
        const char *hl = (strcmp(ifaces[i].name, "lo") == 0) ? COLOR_GRAY : COLOR_GREEN;
        printf("%s%-18s" COLOR_RESET " %-18s %-18s\n", hl, ifaces[i].name, rxS, txS);
    }
    printDivider();

    printf("\n" COLOR_BOLD "Options:" COLOR_RESET "\n");
    printf("  0. Start Real-Time Bandwidth Monitor (KB/s live)\n");
    printf("  1. Return to Main Menu\n");
    printf("Enter choice: ");
    int choice;
    if (scanf("%d", &choice) != 1 || choice != 0) { while (getchar() != '\n'); return; }

    printf("\nLive bandwidth active. Press " COLOR_YELLOW "'q', ESC, or End" COLOR_RESET " to stop.\n");
    sleep(1);
    enableRawMode();
    NetInterface prev[16];
    int prevCount = readNetDev(prev, 16);

    while (1) {
        if (checkExitKey(1000)) break;
        NetInterface curr[16];
        int currCount = readNetDev(curr, 16);
        printf("\033[H\033[2J");
        printHeader("REAL-TIME NETWORK SPEED MONITOR (1s refresh)");
        printf("%-18s %-18s %-18s\n", "INTERFACE", "DOWNLOAD SPEED", "UPLOAD SPEED");
        printDivider();
        for (int i = 0; i < currCount && i < prevCount; i++) {
            if (strcmp(curr[i].name, prev[i].name) == 0) {
                double rxRate = (double)(curr[i].rxBytes - prev[i].rxBytes) / 1024.0;
                double txRate = (double)(curr[i].txBytes - prev[i].txBytes) / 1024.0;
                printf(COLOR_BOLD "%-18s" COLOR_RESET " " COLOR_GREEN "%10.2f KB/s" COLOR_RESET " " COLOR_CYAN "%10.2f KB/s" COLOR_RESET "\n",
                       curr[i].name, rxRate, txRate);
                prev[i] = curr[i];
            }
        }
        printDivider();
        printf("Press " COLOR_YELLOW "'q' or ESC" COLOR_RESET " to return.\n");
        fflush(stdout);
    }
    disableRawMode();
    printf("\n" COLOR_GREEN "Live network monitor ended." COLOR_RESET "\n");
}

/* 9. Top Resource Processes */
int readProcessMemory(int pid, unsigned long long *memoryKB) {
    char path[256]; snprintf(path, sizeof(path), "/proc/%d/statm", pid);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    unsigned long long totalPages, residentPages;
    if (fscanf(f, "%llu %llu", &totalPages, &residentPages) != 2) { fclose(f); return 0; }
    fclose(f);
    long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize <= 0) pageSize = 4096;
    *memoryKB = (residentPages * (unsigned long long)pageSize) / 1024ULL;
    return 1;
}

static int cmpCPU(const void *a, const void *b) {
    const ResourceProcess *p1 = (const ResourceProcess *)a;
    const ResourceProcess *p2 = (const ResourceProcess *)b;
    if (p2->cpuUsage > p1->cpuUsage) return 1;
    if (p2->cpuUsage < p1->cpuUsage) return -1;
    return 0;
}
static int cmpMem(const void *a, const void *b) {
    const ResourceProcess *p1 = (const ResourceProcess *)a;
    const ResourceProcess *p2 = (const ResourceProcess *)b;
    if (p2->memoryKB > p1->memoryKB) return 1;
    if (p2->memoryKB < p1->memoryKB) return -1;
    return 0;
}

void topResourceProcesses(void) {
    ResourceProcess *procList = malloc(sizeof(ResourceProcess) * MAX_PROCESSES);
    unsigned long long *firstCPU = malloc(sizeof(unsigned long long) * MAX_PROCESSES);
    if (!procList || !firstCPU) { free(procList); free(firstCPU); return; }

    printf(COLOR_CYAN "\nSampling process resource metrics (300ms)...\n" COLOR_RESET);
    DIR *dir = opendir("/proc");
    if (!dir) { free(procList); free(firstCPU); return; }

    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && count < MAX_PROCESSES) {
        if (!isdigit((unsigned char)entry->d_name[0])) continue;
        int pid = atoi(entry->d_name);
        unsigned long long ctime; char name[MAX_NAME];
        if (readProcStat(pid, name, sizeof(name), NULL, NULL, &ctime)) {
            procList[count].pid = pid;
            snprintf(procList[count].name, sizeof(procList[count].name), "%s", name);
            firstCPU[count++] = ctime;
        }
    }
    closedir(dir);

    unsigned long long totalCPU1 = getTotalCPUTime();
    usleep(300000);
    unsigned long long totalCPU2 = getTotalCPUTime();
    unsigned long long dTotalCPU = (totalCPU2 > totalCPU1) ? (totalCPU2 - totalCPU1) : 1;

    for (int i = 0; i < count; i++) {
        unsigned long long ctime2 = 0;
        if (readProcStat(procList[i].pid, NULL, 0, NULL, NULL, &ctime2) && ctime2 >= firstCPU[i]) {
            procList[i].cpuUsage = (100.0 * (double)(ctime2 - firstCPU[i])) / (double)dTotalCPU;
        } else {
            procList[i].cpuUsage = 0.0;
        }
        unsigned long long memKB = 0;
        readProcessMemory(procList[i].pid, &memKB);
        procList[i].memoryKB = memKB;
        procList[i].memoryUsage = (double)memKB / 1024.0;
    }

    printHeader("TOP RESOURCE-CONSUMING PROCESSES");
    qsort(procList, count, sizeof(ResourceProcess), cmpCPU);
    printSection("TOP 10 BY CPU");
    printf("%-8s %-24s %-10s\n", "PID", "COMMAND", "CPU %");
    printDivider();
    int cpuLimit = count < 10 ? count : 10;
    for (int i = 0; i < cpuLimit; i++) {
        printf("%-8d %-24s %6.2f%% ", procList[i].pid, procList[i].name, procList[i].cpuUsage);
        printProgressBar("", procList[i].cpuUsage, 12);
    }

    qsort(procList, count, sizeof(ResourceProcess), cmpMem);
    printSection("TOP 10 BY MEMORY (RSS)");
    printf("%-8s %-24s %-12s\n", "PID", "COMMAND", "MEMORY (MB)");
    printDivider();
    int memLimit = count < 10 ? count : 10;
    for (int i = 0; i < memLimit; i++) {
        printf("%-8d %-24s %8.2f MB\n", procList[i].pid, procList[i].name, procList[i].memoryUsage);
    }
    printDivider();
    free(procList); free(firstCPU);
}

/* 10. Process Search */
void processSearch(void) {
    char term[128];
    printHeader("PROCESS SEARCH");
    printf("Enter process name or PID: ");
    while (getchar() != '\n');
    if (!fgets(term, sizeof(term), stdin)) return;
    term[strcspn(term, "\r\n")] = '\0';
    if (strlen(term) == 0) return;

    DIR *dir = opendir("/proc");
    if (!dir) return;

    struct dirent *entry;
    int found = 0, lastPid = -1;
    while ((entry = readdir(dir)) != NULL) {
        if (!isdigit((unsigned char)entry->d_name[0])) continue;
        int pid = atoi(entry->d_name);
        char name[MAX_NAME], st = '?';
        int ppid = 0;
        if (readProcStat(pid, name, sizeof(name), &st, &ppid, NULL)) {
            char pidStr[32]; snprintf(pidStr, sizeof(pidStr), "%d", pid);
            if (strcasestr(name, term) || strcmp(pidStr, term) == 0) {
                if (!found) {
                    printf("\n%-8s %-8s %-7s %-28s\n", "PID", "PPID", "STATE", "PROCESS NAME");
                    printDivider();
                }
                printf(COLOR_BOLD "%-8d" COLOR_RESET " %-8d %-7c %-28s\n", pid, ppid, st, name);
                found++; lastPid = pid;
            }
        }
    }
    closedir(dir);

    if (found == 0) {
        printf(COLOR_YELLOW "No matching processes found for '%s'.\n" COLOR_RESET, term);
    } else {
        printf("\nFound " COLOR_GREEN "%d" COLOR_RESET " matching processes.\n", found);
        if (found == 1 && lastPid > 0) {
            printf("\nManage PID %d? (1 = Yes, 0 = No): ", lastPid);
            int m;
            if (scanf("%d", &m) == 1 && m == 1) processManager();
        }
    }
    printDivider();
}

/* 11. Interactive Process Manager */
void processManager(void) {
    printHeader("PROCESS MANAGER (SIGNALS & PRIORITY)");
    printf("Enter Target Process PID: ");
    int pid;
    if (scanf("%d", &pid) != 1 || pid <= 0) {
        while (getchar() != '\n');
        printf(COLOR_RED "Invalid PID.\n" COLOR_RESET);
        return;
    }

    char path[256]; snprintf(path, sizeof(path), "/proc/%d", pid);
    if (access(path, F_OK) != 0) {
        printf(COLOR_RED "PID %d does not exist or has already exited.\n" COLOR_RESET, pid);
        return;
    }

    printf("\n" COLOR_BOLD "Actions for PID %d:" COLOR_RESET "\n", pid);
    printf("  1. Graceful Terminate (" COLOR_YELLOW "SIGTERM - 15" COLOR_RESET ")\n");
    printf("  2. Force Kill         (" COLOR_RED "SIGKILL - 9" COLOR_RESET ")\n");
    printf("  3. Pause Process      (" COLOR_BLUE "SIGSTOP - 19" COLOR_RESET ")\n");
    printf("  4. Resume Process     (" COLOR_GREEN "SIGCONT - 18" COLOR_RESET ")\n");
    printf("  5. Change Priority    (renice)\n");
    printf("  6. Cancel\n");
    printf("Select action (1-6): ");

    int act;
    if (scanf("%d", &act) != 1) { while (getchar() != '\n'); return; }

    switch (act) {
        case 1:
            if (kill(pid, SIGTERM) == 0) printf(COLOR_GREEN "SIGTERM sent to PID %d.\n" COLOR_RESET, pid);
            else printf(COLOR_RED "Failed: %s\n" COLOR_RESET, strerror(errno));
            break;
        case 2:
            if (kill(pid, SIGKILL) == 0) printf(COLOR_GREEN "SIGKILL sent to PID %d.\n" COLOR_RESET, pid);
            else printf(COLOR_RED "Failed: %s\n" COLOR_RESET, strerror(errno));
            break;
        case 3:
            if (kill(pid, SIGSTOP) == 0) printf(COLOR_BLUE "Process %d paused (SIGSTOP).\n" COLOR_RESET, pid);
            else printf(COLOR_RED "Failed: %s\n" COLOR_RESET, strerror(errno));
            break;
        case 4:
            if (kill(pid, SIGCONT) == 0) printf(COLOR_GREEN "Process %d resumed (SIGCONT).\n" COLOR_RESET, pid);
            else printf(COLOR_RED "Failed: %s\n" COLOR_RESET, strerror(errno));
            break;
        case 5: {
            printf("Enter new nice priority (-20 to 19): ");
            int prio;
            if (scanf("%d", &prio) == 1) {
                if (prio < -20 || prio > 19) printf(COLOR_RED "Nice must be between -20 and 19.\n" COLOR_RESET);
                else if (setpriority(PRIO_PROCESS, pid, prio) == 0) printf(COLOR_GREEN "Priority set to %d.\n" COLOR_RESET, prio);
                else printf(COLOR_RED "Failed: %s (need sudo for negative nice)\n" COLOR_RESET, strerror(errno));
            }
            break;
        }
        default: printf("Action cancelled.\n"); break;
    }
}

/* 12. System Load Average & Kernel Scheduling */
void systemLoadAndScheduling(void) {
    FILE *f = fopen("/proc/loadavg", "r");
    if (!f) {
        printf(COLOR_RED "Unable to read /proc/loadavg.\n" COLOR_RESET);
        return;
    }

    double load1 = 0.0, load5 = 0.0, load15 = 0.0;
    int runEntities = 0, totalEntities = 0, lastPid = 0;

    if (fscanf(f, "%lf %lf %lf %d/%d %d",
               &load1, &load5, &load15, &runEntities, &totalEntities, &lastPid) < 5) {
        fclose(f);
        printf(COLOR_RED "Failed to parse /proc/loadavg.\n" COLOR_RESET);
        return;
    }
    fclose(f);

    long cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (cores <= 0) cores = 1;

    printHeader("SYSTEM LOAD AVERAGE & KERNEL SCHEDULING");
    printSection("CPU LOAD AVERAGES (Multi-Core Normalized)");

    double pct1 = (load1 / (double)cores) * 100.0;
    double pct5 = (load5 / (double)cores) * 100.0;
    double pct15 = (load15 / (double)cores) * 100.0;

    printf("%-20s : %.2f (%.1f%% per core)\n", "1-Minute Load", load1, pct1);
    printProgressBar("1m Load Gauge", pct1 > 100.0 ? 100.0 : pct1, 22);

    printf("%-20s : %.2f (%.1f%% per core)\n", "5-Minute Load", load5, pct5);
    printProgressBar("5m Load Gauge", pct5 > 100.0 ? 100.0 : pct5, 22);

    printf("%-20s : %.2f (%.1f%% per core)\n", "15-Minute Load", load15, pct15);
    printProgressBar("15m Load Gauge", pct15 > 100.0 ? 100.0 : pct15, 22);

    printSection("SCHEDULER & THREAD POOL METRICS");
    printf("%-24s : " COLOR_GREEN "%d" COLOR_RESET " active / %d total\n", "Runnable Threads", runEntities, totalEntities);
    printf("%-24s : %d\n", "Last Spawned PID", lastPid);
    printf("%-24s : %ld online\n", "Configured CPU Cores", cores);

    FILE *sf = fopen("/proc/stat", "r");
    if (sf) {
        char line[256];
        unsigned long long ctxt = 0, procs = 0;
        int procsRunning = 0, procsBlocked = 0;

        while (fgets(line, sizeof(line), sf)) {
            if (strncmp(line, "ctxt ", 5) == 0) sscanf(line + 5, "%llu", &ctxt);
            else if (strncmp(line, "processes ", 10) == 0) sscanf(line + 10, "%llu", &procs);
            else if (strncmp(line, "procs_running ", 14) == 0) sscanf(line + 14, "%d", &procsRunning);
            else if (strncmp(line, "procs_blocked ", 14) == 0) sscanf(line + 14, "%d", &procsBlocked);
        }
        fclose(sf);

        printSection("CONTEXT SWITCHES & PROCESS CONCURRENCY");
        printf("%-24s : %llu\n", "Total Context Switches", ctxt);
        printf("%-24s : %llu\n", "Total Forks Since Boot", procs);
        printf("%-24s : %d\n", "Processes In Run Queue", procsRunning);
        printf("%-24s : %d (waiting for I/O completion)\n", "Processes Blocked", procsBlocked);
    }
    printDivider();
}

/* 13. Linux Kernel Modules (LKM) & Drivers */
void kernelModulesExplorer(void) {
    FILE *f = fopen("/proc/modules", "r");
    if (!f) {
        printf(COLOR_RED "Unable to read /proc/modules. Kernel module inspection restricted.\n" COLOR_RESET);
        return;
    }

    printHeader("LINUX KERNEL MODULES (LKM) & DRIVERS");

    char line[512];
    int count = 0;
    unsigned long long totalMemBytes = 0;

    struct ModuleEntry {
        char name[64];
        unsigned long size;
        int instances;
        char state[16];
    } modules[20];

    while (fgets(line, sizeof(line), f)) {
        char mname[64], mstate[16];
        unsigned long msize = 0;
        int minst = 0;

        if (sscanf(line, "%63s %lu %d %*s %15s", mname, &msize, &minst, mstate) >= 3) {
            totalMemBytes += msize;
            if (count < 20) {
                snprintf(modules[count].name, sizeof(modules[count].name), "%s", mname);
                modules[count].size = msize;
                modules[count].instances = minst;
                snprintf(modules[count].state, sizeof(modules[count].state), "%s", mstate);
            }
            count++;
        }
    }
    fclose(f);

    printf(COLOR_BOLD "Kernel Subsystem Summary:" COLOR_RESET " %d Loaded Modules | Memory: %.2f MB\n\n",
           count, (double)totalMemBytes / (1024.0 * 1024.0));

    printf("%-24s %-12s %-10s %-10s\n", "MODULE NAME", "SIZE (KB)", "USED BY", "STATE");
    printDivider();

    int limit = count < 20 ? count : 20;
    for (int i = 0; i < limit; i++) {
        printf("%-24s %8.1f KB %8d %s%10s" COLOR_RESET "\n",
               modules[i].name,
               (double)modules[i].size / 1024.0,
               modules[i].instances,
               COLOR_GREEN,
               modules[i].state);
    }

    if (count > 20) {
        printf(COLOR_GRAY "... and %d additional kernel modules loaded.\n" COLOR_RESET, count - 20);
    }

    printf("\n" COLOR_BOLD "Options:" COLOR_RESET "\n");
    printf("  1. Search for a specific kernel module\n");
    printf("  2. Continue\n");
    printf("Enter choice: ");

    int subchoice;
    if (scanf("%d", &subchoice) == 1 && subchoice == 1) {
        char term[64];
        printf("Enter module name substring: ");
        while (getchar() != '\n');
        if (fgets(term, sizeof(term), stdin)) {
            term[strcspn(term, "\r\n")] = '\0';
            FILE *rf = fopen("/proc/modules", "r");
            if (rf) {
                printSection("MATCHING KERNEL MODULES");
                printf("%-24s %-12s %-10s %-10s\n", "MODULE NAME", "SIZE (KB)", "USED BY", "STATE");
                printDivider();
                int matches = 0;
                while (fgets(line, sizeof(line), rf)) {
                    char mname[64], mstate[16];
                    unsigned long msize = 0;
                    int minst = 0;
                    if (sscanf(line, "%63s %lu %d %*s %15s", mname, &msize, &minst, mstate) >= 3) {
                        if (strcasestr(mname, term)) {
                            printf(COLOR_BOLD "%-24s" COLOR_RESET " %8.1f KB %8d %10s\n",
                                   mname, (double)msize / 1024.0, minst, mstate);
                            matches++;
                        }
                    }
                }
                fclose(rf);
                if (matches == 0) {
                    printf(COLOR_YELLOW "No kernel modules matching '%s' found.\n" COLOR_RESET, term);
                }
            }
        }
    } else {
        while (getchar() != '\n');
    }
    printDivider();
}

/* 14. Resource Alerts */
void resourceAlerts(void) {
    double cpu = 0.0, mem = 0.0, disk = 0.0;
    int cOK = getCPUUsage(&cpu), mOK = getMemoryUsage(&mem), dOK = getDiskUsage(&disk);

    printHeader("RESOURCE THRESHOLD ALERTS");
    printSection("ACTIVE TELEMETRY & WARNING CHECKS");
    int alert = 0;

    if (cOK) {
        printf("CPU Usage: %5.1f%% ", cpu); printStatusBadge(cpu);
        if (cpu > 85.0) { printf(COLOR_RED " -> ALERT: Critical CPU load!\n" COLOR_RESET); alert = 1; }
        else if (cpu > 70.0) { printf(COLOR_YELLOW " -> NOTICE: Elevated CPU load.\n" COLOR_RESET); alert = 1; }
        else printf("\n");
    }
    if (mOK) {
        printf("RAM Usage: %5.1f%% ", mem); printStatusBadge(mem);
        if (mem > 85.0) { printf(COLOR_RED " -> ALERT: Memory pressure critical!\n" COLOR_RESET); alert = 1; }
        else if (mem > 70.0) { printf(COLOR_YELLOW " -> NOTICE: High memory pressure.\n" COLOR_RESET); alert = 1; }
        else printf("\n");
    }
    if (dOK) {
        printf("Disk (/): %5.1f%% ", disk); printStatusBadge(disk);
        if (disk > 85.0) { printf(COLOR_RED " -> ALERT: Low disk space!\n" COLOR_RESET); alert = 1; }
        else if (disk > 70.0) { printf(COLOR_YELLOW " -> NOTICE: Root disk filling up.\n" COLOR_RESET); alert = 1; }
        else printf("\n");
    }
    if (!alert) printf("\n" COLOR_GREEN "All system metrics are operating within safe parameters." COLOR_RESET "\n");
    printDivider();
}

/* 15. Resource Logging */
void resourceLogging(void) {
    double cpu = 0.0, mem = 0.0, disk = 0.0;
    int cOK = getCPUUsage(&cpu), mOK = getMemoryUsage(&mem), dOK = getDiskUsage(&disk);

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char timeStr[64] = "Unknown Time";
    if (tm) strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", tm);

    FILE *f = fopen(LOG_FILE, "a");
    if (!f) return;
    fprintf(f, "[%s] CPU: %5.1f%% | MEM: %5.1f%% | DISK: %5.1f%%\n",
            timeStr, cOK ? cpu : -1.0, mOK ? mem : -1.0, dOK ? disk : -1.0);
    fclose(f);

    printHeader("RESOURCE LOGGING FACILITY");
    printf(COLOR_GREEN "Logged system snapshot to '%s'." COLOR_RESET "\n", LOG_FILE);
    printf("Timestamp : %s\n", timeStr);
    printf("Recorded  : CPU %5.1f%% | RAM %5.1f%% | DISK %5.1f%%\n", cpu, mem, disk);

    printf("\n" COLOR_BOLD "Options:" COLOR_RESET " [1] View Last 10 Log Entries  |  [2] Return\nEnter choice: ");
    int choice;
    if (scanf("%d", &choice) == 1 && choice == 1) {
        FILE *rf = fopen(LOG_FILE, "r");
        if (rf) {
            char lines[10][256]; int count = 0; char buf[256];
            while (fgets(buf, sizeof(buf), rf)) {
                snprintf(lines[count % 10], sizeof(lines[0]), "%s", buf);
                count++;
            }
            fclose(rf);
            printSection("RECENT LOG ENTRIES");
            int start = (count > 10) ? (count - 10) : 0;
            for (int i = start; i < count; i++) printf("  %s", lines[i % 10]);
        }
    } else {
        while (getchar() != '\n');
    }
    printDivider();
}

void afterFeature(void) {
    printf("\n" COLOR_BOLD "Action:" COLOR_RESET " [1] Return to Main Menu  |  [2] Exit\nEnter choice: ");
    int choice;
    if (scanf("%d", &choice) != 1) { while (getchar() != '\n'); return; }
    if (choice == 2) {
        printf("\nExiting Linux System Monitor. Thank you!\n\n");
        exit(0);
    }
}

int main(void) {
    setupSignalHandler();

    while (1) {
        clearScreen();
        printf("\n");
        printf(COLOR_CYAN "======================================================================\n" COLOR_RESET);
        printf("                        " COLOR_BOLD COLOR_WHITE "LINUX SYSTEM MONITOR" COLOR_RESET "\n");
        printf(COLOR_CYAN "======================================================================\n" COLOR_RESET);
        printf("\n");
        printf(COLOR_BLUE "  [ SYSTEM & HARDWARE METRICS ]\n" COLOR_RESET);
        printf("   " COLOR_CYAN " 1." COLOR_RESET " System Information & OS Details\n");
        printf("   " COLOR_CYAN " 2." COLOR_RESET " CPU Usage & Live Frequency Monitor\n");
        printf("   " COLOR_CYAN " 3." COLOR_RESET " Memory Utilization (RAM & Swap)\n");
        printf("   " COLOR_CYAN " 4." COLOR_RESET " Disk & Multi-Mount Storage\n");
        printf("   " COLOR_CYAN " 5." COLOR_RESET " Running Processes & Process Tree\n");
        printf("   " COLOR_CYAN " 6." COLOR_RESET " System Uptime & Cumulative CPU Time\n");
        printf("   " COLOR_CYAN " 7." COLOR_RESET " System Health Summary Dashboard\n");
        printf("\n");
        printf(COLOR_BLUE "  [ PROCESS & NETWORK MANAGEMENT ]\n" COLOR_RESET);
        printf("   " COLOR_CYAN " 8." COLOR_RESET " Network Monitoring & Live Bandwidth\n");
        printf("   " COLOR_CYAN " 9." COLOR_RESET " Top Resource-Consuming Processes\n");
        printf("   " COLOR_CYAN "10." COLOR_RESET " Process Search & Filter\n");
        printf("   " COLOR_CYAN "11." COLOR_RESET " Interactive Process Manager (Kill/Pause/Nice)\n");
        printf("\n");
        printf(COLOR_BLUE "  [ KERNEL DIAGNOSTICS & TELEMETRY ]\n" COLOR_RESET);
        printf("   " COLOR_CYAN "12." COLOR_RESET " System Load Averages & Context Switches\n");
        printf("   " COLOR_CYAN "13." COLOR_RESET " Kernel Modules Explorer (Loaded LKMs)\n");
        printf("   " COLOR_CYAN "14." COLOR_RESET " Resource Threshold Alerts\n");
        printf("   " COLOR_CYAN "15." COLOR_RESET " Resource Usage Logging & History\n");
        printf("   " COLOR_CYAN "16." COLOR_RESET " Exit\n");
        printf("\n" COLOR_GRAY "----------------------------------------------------------------------\n" COLOR_RESET);
        printf(COLOR_BOLD "Enter your choice (1-16): " COLOR_RESET);

        int choice;
        if (scanf("%d", &choice) != 1) {
            printf(COLOR_RED "\nInvalid input. Please enter a valid number.\n" COLOR_RESET);
            while (getchar() != '\n');
            sleep(1);
            continue;
        }

        switch (choice) {
            case 1:  systemInformation();       afterFeature(); break;
            case 2:  cpuUsage();                afterFeature(); break;
            case 3:  memoryUsage();             afterFeature(); break;
            case 4:  diskUsage();               afterFeature(); break;
            case 5:  processMonitoring();       afterFeature(); break;
            case 6:  systemUptime();            afterFeature(); break;
            case 7:  systemHealthSummary();     afterFeature(); break;
            case 8:  networkMonitoring();       afterFeature(); break;
            case 9:  topResourceProcesses();    afterFeature(); break;
            case 10: processSearch();            afterFeature(); break;
            case 11: processManager();           afterFeature(); break;
            case 12: systemLoadAndScheduling();  afterFeature(); break;
            case 13: kernelModulesExplorer();    afterFeature(); break;
            case 14: resourceAlerts();           afterFeature(); break;
            case 15: resourceLogging();          afterFeature(); break;
            case 16:
                printf("\nExiting Linux System Monitor...\nThank you!\n\n");
                return 0;
            default:
                printf(COLOR_RED "\nInvalid choice. Please select between 1 and 16.\n" COLOR_RESET);
                sleep(1);
        }
    }
    return 0;
}