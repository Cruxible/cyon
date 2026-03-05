#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>
#include <sys/ioctl.h>
#define MAX_CMD_LEN 1024
void print_prompt() {
    struct passwd *pw = getpwuid(getuid());
    const char *username = pw ? pw->pw_name : "user";
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd() error");
        strcpy(cwd, "?");
    }
    printf(" \033[96m┌░\033[37m%s\033[96m░\n └░\033[37m%s\033[96m░\033[37m ", cwd, username);
    fflush(stdout);
}
void handle_cd(char *path) {
    if (path == NULL || strcmp(path, "~") == 0) {
        const char *home = getenv("HOME");
        if (!home) {
            struct passwd *pw = getpwuid(getuid());
            if (!pw) {
                fprintf(stderr, "Cannot get home directory\n");
                return;
            }
            home = pw->pw_dir;
        }
        path = (char *)home;
    }
    if (chdir(path) != 0) {
        perror("cd failed");
    } else {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != NULL)
            printf("\033[96mChanged directory to: %s\033[37m\n", cwd);
    }
}
void change_dir() {
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        if (!pw) {
            fprintf(stderr, "Cannot get home directory\n");
            return;
        }
        home = pw->pw_dir;
    }
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/cyon", home);
    if (chdir(path) != 0) {
        perror("chdir failed");
        return;
    }
    printf("\033[96mChanged directory to: %s\033[37m\n", path);
}
void handle_ls() {
    DIR *d;
    struct dirent *dir;
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd failed");
        return;
    }
    d = opendir(cwd);
    if (!d) {
        perror("opendir failed");
        return;
    }
    struct winsize w;
    int term_width = 80;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        term_width = w.ws_col;
    }
    size_t max_len = 0;
    while ((dir = readdir(d)) != NULL) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
            continue;
        size_t len = strlen(dir->d_name);
        if (len > max_len) max_len = len;
    }
    rewinddir(d);
    int col_width = max_len + 2;
    int columns = term_width / col_width;
    if (columns < 1) columns = 1;
    int count = 0;
    while ((dir = readdir(d)) != NULL) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
            continue;
        char fullpath[PATH_MAX];
        if (snprintf(fullpath, sizeof(fullpath), "%s/%s", cwd, dir->d_name) >= PATH_MAX)
            continue;
        struct stat st;
        if (stat(fullpath, &st) == 0) {
            if (dir->d_name[0] == '.')
                printf("\033[90m%-*s", (int)col_width, dir->d_name);
            else if (S_ISDIR(st.st_mode))
                printf("\033[37m%-*s", (int)col_width, dir->d_name);
            else
                printf("\033[96m%-*s", (int)col_width, dir->d_name);
        } else {
            printf("%-*s", (int)col_width, dir->d_name);
        }
        count++;
        if (count % columns == 0)
            printf("\n");
    }
    if (count % columns != 0)
        printf("\n");
    printf("\033[37m");
    closedir(d);
}

void run_netscan() {
    char ip[64], start[16], end[16];
    printf(" Target IP: ");
    fflush(stdout);
    scanf("%63s", ip);
    printf(" Start Port: ");
    fflush(stdout);
    scanf("%15s", start);
    printf(" End Port: ");
    fflush(stdout);
    scanf("%15s", end);
    getchar();
    char cmd[PATH_MAX];
    snprintf(cmd, sizeof(cmd), "./bin/cyon_netscan %s %s %s", ip, start, end);
    system(cmd);
}

int main() {
    change_dir();
    printf("\n \033[96m▄█▄  ▀▄    ▄ ████▄    ▄   \n █▀ ▀▄  █  █  █   █     █  \n █   ▀   ▀█   █   █ ██   █ \n █▄  ▄▀  █    ▀████ █ █  █ \n ▀███▀ ▄▀           █  █ █ \n                    █   ██\n");
    printf("\n\033[94m Cyon CLI Tool - Running standalone.\033[37m\n\n \033[96mtools\n security\n exit\n\n");
    char command[MAX_CMD_LEN];
    while (1) {
        print_prompt();
        if (fgets(command, sizeof(command), stdin) == NULL) {
            printf("\n");
            break;
        }
        command[strcspn(command, "\n")] = 0;
        if (strcmp(command, "exit") == 0)
            break;
        if (strncmp(command, "cd", 2) == 0) {
            char *path = (command[2] == ' ') ? command + 3 : NULL;
            handle_cd(path);
            continue;
        }
        if (strcmp(command, "ls") == 0) {
            handle_ls();
            continue;
        }
        // Tools menu
        if (strcmp(command, "tools") == 0) {
            printf(" Tools menu\n \033[96mpyra\n exit\n\033[37m");
            while (1) {
                print_prompt();
                if (fgets(command, sizeof(command), stdin) == NULL) {
                    printf("\n");
                    break;
                }
                command[strcspn(command, "\n")] = 0;
                if (strcmp(command, "exit") == 0) {
                    printf(" Exiting Tools Menu.\n");
                    break;
                }
                if (strcmp(command, "pyra") == 0) {
                    const char *home = getenv("HOME");
                    if (!home) {
                        fprintf(stderr, "Could not determine HOME directory.\n");
                        continue;
                    }
                    char pyra_path[PATH_MAX];
                    snprintf(pyra_path, sizeof(pyra_path), "%s/cyon/pyra_tool/pyra_toolz", home);
                    pid_t pid = fork();
                    if (pid == 0) {
                        char *args[] = {pyra_path, NULL};
                        execv(pyra_path, args);
                        perror("execv failed");
                        return 1;
                    } else if (pid > 0) {
                        int status;
                        waitpid(pid, &status, 0);
                        printf(" Pyra exited.\n");
                    } else {
                        perror("fork failed");
                        return 1;
                    }
                    continue;
                }
            }
            continue;
        }
        // Security menu
        if (strcmp(command, "security") == 0) {
            printf(" Security menu\n \033[96mnetscan\n exit\n\033[37m");
            while (1) {
                print_prompt();
                if (fgets(command, sizeof(command), stdin) == NULL) {
                    printf("\n");
                    break;
                }
                command[strcspn(command, "\n")] = 0;
                if (strcmp(command, "exit") == 0) {
                    printf(" Exiting Security Menu.\n");
                    break;
                }
                if (strcmp(command, "netscan") == 0) {
                    run_netscan();
                    continue;
                }
            }
            continue;
        }
    }
    printf(" Goodbye!\n");
    return 0;
}
