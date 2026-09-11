#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <pwd.h>

/* 1. System Information */
void systemInformation()
{
    struct utsname info;

    char cpuModel[256] = "Unknown";
    char cpuFrequency[100] = "Unknown";

    long cpuCores;
    char *username;
    char *homeDirectory;

    /* Get basic system information */
    if (uname(&info) != 0)
    {
        printf("Unable to get system information.\n");
        return;
    }

    /* Get CPU information */
    FILE *file = fopen("/proc/cpuinfo", "r");

    if (file != NULL)
    {
        char line[256];

        while (fgets(line, sizeof(line), file) != NULL)
        {
            if (strncmp(line, "model name", 10) == 0)
            {
                char *colon = strchr(line, ':');

                if (colon != NULL)
                {
                    strcpy(cpuModel, colon + 2);
                    cpuModel[strcspn(cpuModel, "\n")] = '\0';
                }
            }

            if (strncmp(line, "cpu MHz", 7) == 0)
            {
                char *colon = strchr(line, ':');

                if (colon != NULL)
                {
                    double frequency;

                    if (sscanf(colon + 1, "%lf", &frequency) == 1)
                    {
                        snprintf(cpuFrequency,
                                 sizeof(cpuFrequency),
                                 "%.2f MHz",
                                 frequency);
                    }
                }

                break;
            }
        }

        fclose(file);
    }

    /* Get number of CPU cores */
    cpuCores = sysconf(_SC_NPROCESSORS_ONLN);

    /* Get current username */
    username = getlogin();

    if (username == NULL)
    {
        username = getenv("USER");
    }

    /* Get home directory */
    homeDirectory = getenv("HOME");

    printf("\n========================================\n");
    printf("          SYSTEM INFORMATION\n");
    printf("========================================\n");

    printf("\n---------- SYSTEM DETAILS ----------\n");

    printf("Operating System    : %s\n",
           info.sysname);

    printf("Kernel Version      : %s\n",
           info.release);

    printf("Architecture        : %s\n",
           info.machine);

    printf("Hostname            : %s\n",
           info.nodename);

    printf("\n---------- CPU INFORMATION ----------\n");

    printf("CPU Model           : %s\n",
           cpuModel);

    if (cpuCores > 0)
    {
        printf("CPU Cores           : %ld\n",
               cpuCores);
    }
    else
    {
        printf("CPU Cores           : Unknown\n");
    }

    printf("CPU Frequency       : %s\n",
           cpuFrequency);

    printf("\n---------- USER INFORMATION ----------\n");

    if (username != NULL)
    {
        printf("Current User        : %s\n",
               username);
    }
    else
    {
        printf("Current User        : Unknown\n");
    }

    if (homeDirectory != NULL)
    {
        printf("Home Directory      : %s\n",
               homeDirectory);
    }
    else
    {
        printf("Home Directory      : Unknown\n");
    }

    printf("\n========================================\n");
}

int endKeyPressed()
{
    fd_set readfds;
    struct timeval timeout;

    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;

    if (select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout) > 0)
    {
        char ch = getchar();

        if (ch == 27)
        {
            char ch2 = getchar();

            if (ch2 == '[')
            {
                char ch3 = getchar();

                if (ch3 == 'F')
                    return 1;

                if (ch3 == '4')
                {
                    char ch4 = getchar();

                    if (ch4 == '~')
                        return 1;
                }
            }
        }
    }

    return 0;
}

/* 2. CPU Usage */
void cpuUsage() {
    FILE *file;
    char line[256];

    long user1, nice1, system1, idle1;
    long iowait1, irq1, softirq1, steal1;

    long user2, nice2, system2, idle2;
    long iowait2, irq2, softirq2, steal2;

    /* Get first CPU reading */
    file = fopen("/proc/stat", "r");

    if (file == NULL) {
        printf("Unable to read CPU information.\n");
        return;
    }

    fgets(line, sizeof(line), file);

    sscanf(line, "cpu %ld %ld %ld %ld %ld %ld %ld %ld",
           &user1, &nice1, &system1, &idle1,
           &iowait1, &irq1, &softirq1, &steal1);

    fclose(file);

    /* Wait one second */
    sleep(1);

    /* Get second CPU reading */
    file = fopen("/proc/stat", "r");

    if (file == NULL) {
        printf("Unable to read CPU information.\n");
        return;
    }

    fgets(line, sizeof(line), file);

    sscanf(line, "cpu %ld %ld %ld %ld %ld %ld %ld %ld",
           &user2, &nice2, &system2, &idle2,
           &iowait2, &irq2, &softirq2, &steal2);

    fclose(file);

    long total1 = user1 + nice1 + system1 + idle1 +
                  iowait1 + irq1 + softirq1 + steal1;

    long total2 = user2 + nice2 + system2 + idle2 +
                  iowait2 + irq2 + softirq2 + steal2;

    long idleTotal1 = idle1 + iowait1;
    long idleTotal2 = idle2 + iowait2;

    long totalDifference = total2 - total1;
    long idleDifference = idleTotal2 - idleTotal1;

    double currentUsage = 0.0;

    if (totalDifference > 0) {
        currentUsage =
            100.0 * (totalDifference - idleDifference)
            / totalDifference;
    }

    printf("\n========== CPU MONITOR ==========\n");
    printf("Current CPU Usage : %.2f%%\n", currentUsage);

    int choice;

printf("\n0. Start CPU Monitoring\n");
printf("1. Return to Main Menu\n");
printf("2. Exit\n");

printf("\nEnter your choice: ");
scanf("%d", &choice);

if (choice == 1)
{
    return;
}

if (choice == 2)
{
    printf("\nExiting Linux System Monitor...\n");
    printf("Thank you!\n\n");
    exit(0);
}

if (choice != 0)
{
    printf("\nInvalid choice. Returning to Main Menu...\n");
    return;
}
struct termios oldTerminal;
struct termios newTerminal;

tcgetattr(STDIN_FILENO, &oldTerminal);

newTerminal = oldTerminal;
newTerminal.c_lflag &= ~(ICANON | ECHO);

tcsetattr(STDIN_FILENO, TCSANOW, &newTerminal);

    printf("\n========== REAL-TIME CPU MONITORING ==========\n");
    printf("Press End key to stop monitoring.\n\n");

    /*
     * Continuous monitoring.
     * Currently Ctrl+C can be used to stop it.
     * We will add the actual END-key handling
     * after testing the CPU monitoring.
     */

    while (1) {
        if (endKeyPressed()) {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldTerminal);
        printf("\n\nCPU monitoring stopped.\n");
        printf("Returning to Main Menu...\n");
        sleep(1);
        return;
    }
        file = fopen("/proc/stat", "r");

        if (file == NULL) {
            printf("Unable to read CPU information.\n");
            return;
        }

        fgets(line, sizeof(line), file);

        sscanf(line, "cpu %ld %ld %ld %ld %ld %ld %ld %ld",
               &user1, &nice1, &system1, &idle1,
               &iowait1, &irq1, &softirq1, &steal1);

        fclose(file);

        sleep(1);

        file = fopen("/proc/stat", "r");

        if (file == NULL) {
            printf("Unable to read CPU information.\n");
            return;
        }

        fgets(line, sizeof(line), file);

        sscanf(line, "cpu %ld %ld %ld %ld %ld %ld %ld %ld",
               &user2, &nice2, &system2, &idle2,
               &iowait2, &irq2, &softirq2, &steal2);

        fclose(file);

        total1 = user1 + nice1 + system1 + idle1 +
                 iowait1 + irq1 + softirq1 + steal1;

        total2 = user2 + nice2 + system2 + idle2 +
                 iowait2 + irq2 + softirq2 + steal2;

        idleTotal1 = idle1 + iowait1;
        idleTotal2 = idle2 + iowait2;

        totalDifference = total2 - total1;
        idleDifference = idleTotal2 - idleTotal1;

        if (totalDifference > 0) {

            double usage =
                100.0 * (totalDifference - idleDifference)
                / totalDifference;

            printf("Current CPU Usage : %6.2f%%\n", usage);
            fflush(stdout);
        }
    }
}

/* 3. Memory Usage */
void memoryUsage()
{
    FILE *file;
    char line[256];

    long totalMemory = 0;
    long availableMemory = 0;
    long cachedMemory = 0;
    long buffers = 0;
    long sharedMemory = 0;
    long activeMemory = 0;
    long inactiveMemory = 0;

    long totalSwap = 0;
    long freeSwap = 0;

    file = fopen("/proc/meminfo", "r");

    if (file == NULL)
    {
        printf("Unable to read memory information.\n");
        return;
    }

    while (fgets(line, sizeof(line), file) != NULL)
    {
        if (strncmp(line, "MemTotal:", 9) == 0)
        {
            sscanf(line, "MemTotal: %ld kB", &totalMemory);
        }
        else if (strncmp(line, "MemAvailable:", 13) == 0)
        {
            sscanf(line, "MemAvailable: %ld kB", &availableMemory);
        }
        else if (strncmp(line, "Cached:", 7) == 0)
        {
            sscanf(line, "Cached: %ld kB", &cachedMemory);
        }
        else if (strncmp(line, "Buffers:", 8) == 0)
        {
            sscanf(line, "Buffers: %ld kB", &buffers);
        }
        else if (strncmp(line, "Shmem:", 6) == 0)
        {
            sscanf(line, "Shmem: %ld kB", &sharedMemory);
        }
        else if (strncmp(line, "Active:", 7) == 0)
        {
            sscanf(line, "Active: %ld kB", &activeMemory);
        }
        else if (strncmp(line, "Inactive:", 9) == 0)
        {
            sscanf(line, "Inactive: %ld kB", &inactiveMemory);
        }
        else if (strncmp(line, "SwapTotal:", 10) == 0)
        {
            sscanf(line, "SwapTotal: %ld kB", &totalSwap);
        }
        else if (strncmp(line, "SwapFree:", 9) == 0)
        {
            sscanf(line, "SwapFree: %ld kB", &freeSwap);
        }
    }

    fclose(file);

    if (totalMemory > 0)
    {
        long usedMemory = totalMemory - availableMemory;

        double memoryPercentage =
            ((double)usedMemory / totalMemory) * 100.0;

        long usedSwap = totalSwap - freeSwap;

        double swapPercentage = 0.0;

        if (totalSwap > 0)
        {
            swapPercentage =
                ((double)usedSwap / totalSwap) * 100.0;
        }

        printf("\n========================================\n");
        printf("          MEMORY INFORMATION\n");
        printf("========================================\n");

        printf("\n---------- RAM USAGE ----------\n");

        printf("Total Memory     : %.2f GB\n",
               totalMemory / (1024.0 * 1024.0));

        printf("Used Memory      : %.2f GB\n",
               usedMemory / (1024.0 * 1024.0));

        printf("Available Memory : %.2f GB\n",
               availableMemory / (1024.0 * 1024.0));

        printf("Memory Usage     : %.2f%%\n",
               memoryPercentage);

        printf("\n---------- MEMORY DETAILS ----------\n");

        printf("Cached Memory    : %.2f GB\n",
               cachedMemory / (1024.0 * 1024.0));

        printf("Buffers          : %.2f GB\n",
               buffers / (1024.0 * 1024.0));

        printf("Shared Memory    : %.2f GB\n",
               sharedMemory / (1024.0 * 1024.0));

        printf("Active Memory    : %.2f GB\n",
               activeMemory / (1024.0 * 1024.0));

        printf("Inactive Memory  : %.2f GB\n",
               inactiveMemory / (1024.0 * 1024.0));

        printf("\n---------- SWAP MEMORY ----------\n");

        printf("Total Swap       : %.2f GB\n",
               totalSwap / (1024.0 * 1024.0));

        printf("Used Swap        : %.2f GB\n",
               usedSwap / (1024.0 * 1024.0));

        printf("Free Swap        : %.2f GB\n",
               freeSwap / (1024.0 * 1024.0));

        printf("Swap Usage       : %.2f%%\n",
               swapPercentage);

        printf("\n---------- MEMORY STATUS ----------\n");

        if (memoryPercentage < 70.0)
        {
            printf("Status           : NORMAL\n");
        }
        else if (memoryPercentage <= 85.0)
        {
            printf("Status           : MODERATE\n");
        }
        else
        {
            printf("Status           : HIGH\n");
        }

        printf("\n========================================\n");
    }
}

/* 4. Disk Usage */
void diskUsage()
{
    struct statvfs disk;

    if (statvfs("/", &disk) != 0)
    {
        printf("Unable to read disk information.\n");
        return;
    }

    /* Disk space calculations */
    unsigned long long total =
        (unsigned long long)disk.f_blocks *
        disk.f_frsize;

    unsigned long long freeSpace =
        (unsigned long long)disk.f_bfree *
        disk.f_frsize;

    unsigned long long used =
        total - freeSpace;

    double diskPercentage =
        ((double)used / total) * 100.0;

    /* Inode calculations */
    unsigned long long totalInodes =
        (unsigned long long)disk.f_files;

    unsigned long long freeInodes =
        (unsigned long long)disk.f_ffree;

    unsigned long long usedInodes =
        totalInodes - freeInodes;

    double inodePercentage = 0.0;

    if (totalInodes > 0)
    {
        inodePercentage =
            ((double)usedInodes / totalInodes) * 100.0;
    }

    printf("\n========================================\n");
    printf("           DISK INFORMATION\n");
    printf("========================================\n");

    printf("\n---------- DISK SPACE ----------\n");

    printf("Total Disk Space : %.2f GB\n",
           total / (1024.0 * 1024.0 * 1024.0));

    printf("Used Disk Space  : %.2f GB\n",
           used / (1024.0 * 1024.0 * 1024.0));

    printf("Free Disk Space  : %.2f GB\n",
           freeSpace / (1024.0 * 1024.0 * 1024.0));

    printf("Disk Usage       : %.2f%%\n",
           diskPercentage);

    printf("\n---------- DISK STATUS ----------\n");

    if (diskPercentage < 70.0)
    {
        printf("Status           : NORMAL\n");
    }
    else if (diskPercentage <= 85.0)
    {
        printf("Status           : MODERATE\n");
    }
    else
    {
        printf("Status           : HIGH\n");
    }

    printf("\n---------- INODE INFORMATION ----------\n");

    printf("Total Inodes     : %llu\n",
           totalInodes);

    printf("Used Inodes      : %llu\n",
           usedInodes);

    printf("Free Inodes      : %llu\n",
           freeInodes);

    printf("Inode Usage      : %.2f%%\n",
           inodePercentage);

    printf("\n========================================\n");
}

typedef struct
{
    int pid;
    int ppid;
    char state;
    char name[256];
} ProcessInfo;


void printProcessTree(ProcessInfo processes[], int count,
                      int parentPid, int level)
{
    for (int i = 0; i < count; i++)
    {
        if (processes[i].ppid == parentPid)
        {
            for (int j = 0; j < level; j++)
            {
                printf("    ");
            }

            if (level > 0)
            {
                printf("|-- ");
            }

            printf("%s (PID: %d)\n",
                   processes[i].name,
                   processes[i].pid);

            printProcessTree(processes,
                             count,
                             processes[i].pid,
                             level + 1);
        }
    }
}

/* 5. Running Processes */
void processMonitoring()
{
    DIR *directory;
    struct dirent *entry;

    ProcessInfo processes[4096];
    int processCount = 0;

    int runningCount = 0;
    int sleepingCount = 0;
    int stoppedCount = 0;
    int otherCount = 0;

    directory = opendir("/proc");

    if (directory == NULL)
    {
        printf("Unable to access /proc.\n");
        return;
    }

    while ((entry = readdir(directory)) != NULL)
    {
        if (entry->d_type != DT_DIR)
        {
            continue;
        }

        int isNumber = 1;

        for (int i = 0; entry->d_name[i] != '\0'; i++)
        {
            if (entry->d_name[i] < '0' ||
                entry->d_name[i] > '9')
            {
                isNumber = 0;
                break;
            }
        }

        if (!isNumber)
        {
            continue;
        }

        if (processCount >= 4096)
        {
            break;
        }

        int pid = atoi(entry->d_name);

        char commPath[512];
        char statPath[512];

        snprintf(commPath,
                 sizeof(commPath),
                 "/proc/%s/comm",
                 entry->d_name);

        snprintf(statPath,
                 sizeof(statPath),
                 "/proc/%s/stat",
                 entry->d_name);

        /* Get process name */
        FILE *commFile = fopen(commPath, "r");

        if (commFile == NULL)
        {
            continue;
        }

        if (fgets(processes[processCount].name,
                  sizeof(processes[processCount].name),
                  commFile) == NULL)
        {
            fclose(commFile);
            continue;
        }

        fclose(commFile);

        processes[processCount].name[
            strcspn(processes[processCount].name, "\n")
        ] = '\0';

        /* Get process state and PPID */
        FILE *statFile = fopen(statPath, "r");

        if (statFile == NULL)
        {
            continue;
        }

        int readPid;
        char processState;
        int parentPid;

        if (fscanf(statFile,
                   "%d %*[^)] ) %c %d",
                   &readPid,
                   &processState,
                   &parentPid) != 3)
        {
            fclose(statFile);
            continue;
        }

        fclose(statFile);

        processes[processCount].pid = pid;
        processes[processCount].ppid = parentPid;
        processes[processCount].state = processState;

        /* Count process states */
        if (processState == 'R')
        {
            runningCount++;
        }
        else if (processState == 'S' ||
                 processState == 'D')
        {
            sleepingCount++;
        }
        else if (processState == 'T')
        {
            stoppedCount++;
        }
        else
        {
            otherCount++;
        }

        processCount++;
    }

    closedir(directory);

    /* Process information */
    printf("\n========================================\n");
    printf("          PROCESS INFORMATION\n");
    printf("========================================\n");

    printf("\n%-8s %-8s %-8s %s\n",
           "PID",
           "PPID",
           "STATE",
           "PROCESS");

    printf("------------------------------------------------\n");

    for (int i = 0; i < processCount; i++)
    {
        printf("%-8d %-8d %-8c %s\n",
               processes[i].pid,
               processes[i].ppid,
               processes[i].state,
               processes[i].name);
    }

    /* Process summary */
    printf("\n---------- PROCESS SUMMARY ----------\n");

    printf("Total Processes    : %d\n",
           processCount);

    printf("Running Processes  : %d\n",
           runningCount);

    printf("Sleeping Processes : %d\n",
           sleepingCount);

    printf("Stopped Processes  : %d\n",
           stoppedCount);

    printf("Other Processes    : %d\n",
           otherCount);

    /* Process tree */
    printf("\n---------- PROCESS TREE ----------\n");

    /*
     * Linux normally starts the process tree
     * from PID 1.
     */
    printProcessTree(processes,
                     processCount,
                     0,
                     0);

    printf("\n========================================\n");
}

/* 6. System Uptime */
void systemUptime()
{
    FILE *file;
    double uptimeSeconds;
    double cpuUserTime;
    double cpuNiceTime;
    double cpuSystemTime;
    double cpuIdleTime;
    double cpuIowaitTime;
    double cpuIrqTime;
    double cpuSoftirqTime;
    double cpuStealTime;

    /* Read system uptime */
    file = fopen("/proc/uptime", "r");

    if (file == NULL)
    {
        printf("Unable to read system uptime.\n");
        return;
    }

    if (fscanf(file, "%lf", &uptimeSeconds) != 1)
    {
        fclose(file);
        printf("Unable to read system uptime.\n");
        return;
    }

    fclose(file);

    /* Convert uptime into days, hours, minutes and seconds */
    long totalSeconds = (long)uptimeSeconds;

    long days = totalSeconds / 86400;
    long remainingSeconds = totalSeconds % 86400;

    long hours = remainingSeconds / 3600;
    remainingSeconds %= 3600;

    long minutes = remainingSeconds / 60;
    long seconds = remainingSeconds % 60;

    /* Read CPU time since boot */
    file = fopen("/proc/stat", "r");

    double totalCPUTime = 0.0;

    if (file != NULL)
    {
        char line[256];

        if (fgets(line, sizeof(line), file) != NULL)
        {
            sscanf(line,
                   "cpu %lf %lf %lf %lf %lf %lf %lf %lf",
                   &cpuUserTime,
                   &cpuNiceTime,
                   &cpuSystemTime,
                   &cpuIdleTime,
                   &cpuIowaitTime,
                   &cpuIrqTime,
                   &cpuSoftirqTime,
                   &cpuStealTime);

            totalCPUTime =
                cpuUserTime +
                cpuNiceTime +
                cpuSystemTime +
                cpuIdleTime +
                cpuIowaitTime +
                cpuIrqTime +
                cpuSoftirqTime +
                cpuStealTime;

            /*
             * Linux CPU times are normally measured
             * in clock ticks. Convert ticks to seconds.
             */
            long clockTicks = sysconf(_SC_CLK_TCK);

            if (clockTicks > 0)
            {
                totalCPUTime =
                    totalCPUTime / clockTicks;
            }
        }

        fclose(file);
    }

    /* Get number of CPU cores */
    long cpuCores = sysconf(_SC_NPROCESSORS_ONLN);

    printf("\n========================================\n");
    printf("          SYSTEM UPTIME\n");
    printf("========================================\n");

    printf("\n---------- UPTIME ----------\n");

    printf("Uptime               : %ld Days, %ld Hours, %ld Minutes, %ld Seconds\n",
           days,
           hours,
           minutes,
           seconds);

    printf("Total Uptime Hours   : %.2f Hours\n",
           uptimeSeconds / 3600.0);

    printf("Total Uptime Minutes : %.2f Minutes\n",
           uptimeSeconds / 60.0);

    printf("Total Uptime Seconds : %.0f Seconds\n",
           uptimeSeconds);

    printf("\n---------- SYSTEM STATUS ----------\n");

    printf("System Status        : RUNNING\n");

    printf("\n---------- CPU INFORMATION ----------\n");

    if (cpuCores > 0)
    {
        printf("CPU Cores            : %ld\n",
               cpuCores);
    }

    printf("CPU Time Since Boot  : %.2f Seconds\n",
           totalCPUTime);

    printf("\n========================================\n");
}

/* After Feature Menu */
void afterFeature()
{
    int choice;

    printf("\n----------------------------------------\n");
    printf("1. Return to Main Menu\n");
    printf("2. Exit\n");
    printf("----------------------------------------\n");

    printf("Enter your choice: ");

    if (scanf("%d", &choice) != 1)
    {
        printf("\nInvalid input. Returning to Main Menu...\n");

        while (getchar() != '\n')
        {
            /* Clear invalid input */
        }

        return;
    }

    if (choice == 2)
    {
        printf("\nExiting Linux System Monitor...\n");
        printf("Thank you!\n\n");
        exit(0);
    }

    if (choice != 1)
    {
        printf("\nInvalid choice. Returning to Main Menu...\n");
    }
}

/* Clear Screen */
void clearScreen()
{
    printf("\033[2J\033[H");
}

/* Main Function */
int main()
{
    int choice;

    while (1)
    {
        clearScreen();
        printf("\n");
        printf("========================================\n");
        printf("        LINUX SYSTEM MONITOR\n");
        printf("========================================\n");
        printf("  Monitor and analyze system resources\n");
        printf("========================================\n");

        printf("\n");
        printf("  1. System Information\n");
        printf("  2. CPU Usage\n");
        printf("  3. Memory Usage\n");
        printf("  4. Disk Usage\n");
        printf("  5. Running Processes\n");
        printf("  6. System Uptime\n");
        printf("  7. Exit\n");

        printf("\n----------------------------------------\n");
        printf("Enter your choice: ");

        if (scanf("%d", &choice) != 1)
        {
            printf("\nInvalid input. Please enter a number.\n");

            while (getchar() != '\n')
            {
                /* Clear invalid input */
            }

            continue;
        }

        switch (choice)
        {
            case 1:
                systemInformation();
                afterFeature();
                break;

            case 2:
                cpuUsage();
                break;

            case 3:
                memoryUsage();
                afterFeature();
                break;

            case 4:
                diskUsage();
                afterFeature();
                break;

            case 5:
                processMonitoring();
                afterFeature();
                break;

            case 6:
                systemUptime();
                afterFeature();
                break;

            case 7:
                printf("\nExiting Linux System Monitor...\n");
                printf("Thank you!\n\n");
                return 0;

            default:
                printf("\nInvalid choice. Please select 1-7.\n");
        }
    }

    return 0;
}