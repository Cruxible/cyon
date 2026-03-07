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
#include <pthread.h>
#include <time.h>
#define MAX_CMD_LEN 1024

/* ── Glitch thread ─────────────────────────────────────────────────────────── */
static volatile int glitch_running = 0;
static pthread_t    glitch_thread;
static char         glitch_cwd[PATH_MAX];
static char         glitch_user[64];

static const char GLITCH_CHARS[] = "!@#$%^&*<>?/|\\~`░▒▓";
#define GLITCH_CHAR_COUNT 20

static const char *GHOST_WORDS[] = {
    "pyra", "pyra_env", "pyra_bin", "pyra_tool", "pyra_lib"
};
#define GHOST_WORD_COUNT 5

static void *glitch_fn(void *arg) {
    (void)arg;
    srand((unsigned)time(NULL));

    /* build a plain copy of the TOP line only for glitching */
    char base[PATH_MAX + 16];
    snprintf(base, sizeof(base), " ┌░%s░", glitch_cwd);
    int base_len = (int)strlen(base);

    while (glitch_running) {
        /* sleep a random 80–300ms between glitches */
        int delay_ms = 80 + rand() % 220;
        struct timespec ts = { 0, delay_ms * 1000000L };
        nanosleep(&ts, NULL);
        if (!glitch_running) break;

        /* 1 in 4 chance: flash a ghost word in red instead of char glitch */
        if (rand() % 4 == 0) {
            const char *word = GHOST_WORDS[rand() % GHOST_WORD_COUNT];
            int word_len = (int)strlen(word);
            int max_pos = base_len - word_len;
            if (max_pos < 1) max_pos = 1;
            int pos = rand() % max_pos;

            char glitched[PATH_MAX + 16];
            strncpy(glitched, base, sizeof(glitched) - 1);
            /* scatter a couple char glitches around it too */
            for (int i = 0; i < 2; i++) {
                int cp = rand() % base_len;
                glitched[cp] = GLITCH_CHARS[rand() % GLITCH_CHAR_COUNT];
            }
            /* stamp the ghost word in */
            for (int i = 0; i < word_len && pos + i < base_len; i++)
                glitched[pos + i] = word[i];

            printf("\0337\033[1A\r \033[31m%s\033[37m\0338", glitched);
            fflush(stdout);
        } else {
            /* normal char glitch */
            char glitched[PATH_MAX + 16];
            strncpy(glitched, base, sizeof(glitched) - 1);
            int hits = 1 + rand() % 2;
            for (int i = 0; i < hits; i++) {
                int pos = rand() % base_len;
                glitched[pos] = GLITCH_CHARS[rand() % GLITCH_CHAR_COUNT];
            }
            printf("\0337\033[1A\r \033[35m%s\033[37m\0338", glitched);
            fflush(stdout);
        }

        /* hold the glitch briefly */
        struct timespec hold = { 0, 60000000L }; /* 60ms */
        nanosleep(&hold, NULL);
        if (!glitch_running) break;

        /* restore top line to normal */
        printf("\0337\033[1A\r \033[96m┌░\033[37m%s\033[96m░\033[37m\0338", glitch_cwd);
        fflush(stdout);
    }
    return NULL;
}

static void start_glitch(const char *cwd, const char *user) {
    strncpy(glitch_cwd,  cwd,  sizeof(glitch_cwd)  - 1);
    strncpy(glitch_user, user, sizeof(glitch_user) - 1);
    glitch_running = 1;
    pthread_create(&glitch_thread, NULL, glitch_fn, NULL);
}

static void stop_glitch(void) {
    glitch_running = 0;
    pthread_join(glitch_thread, NULL);
}

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
    start_glitch(cwd, username);
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
            stop_glitch();
            printf("\n");
            break;
        }
        stop_glitch();
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
                    stop_glitch();
                    printf("\n");
                    break;
                }
                stop_glitch();
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
                    stop_glitch();
                    printf("\n");
                    break;
                }
                stop_glitch();
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
