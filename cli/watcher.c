#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <libgen.h>
#include <time.h>
//pkill -x watcher

#define BUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))

void log_event(const char *msg) {
    FILE *log = fopen("dotwatch.log", "a");
    if (!log) return;
    time_t now = time(NULL);
    fprintf(log, "[%s] %s\n", strtok(ctime(&now), "\n"), msg);
    fclose(log);
}

int main() {
    const char *watched_dirs[] = {
        ".ssh",
        ".", // for .bashrc, .bash_history
    };
    const int dir_count = sizeof(watched_dirs) / sizeof(watched_dirs[0]);

    const char *watched_files[] = {
        ".bashrc",
        ".bash_history",
        ".ssh/config",
        ".ssh/authorized_keys",
        ".ssh/known_hosts",
    };
    const int file_count = sizeof(watched_files) / sizeof(watched_files[0]);

    const char *home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "HOME not set\n");
        return 1;
    }

    int fd = inotify_init1(IN_NONBLOCK);
    if (fd < 0) {
        perror("inotify_init1");
        return 1;
    }

    int wd_dirs[dir_count];
    char dir_paths[dir_count][PATH_MAX];

    for (int i = 0; i < dir_count; ++i) {
        snprintf(dir_paths[i], PATH_MAX, "%s/%s", home, watched_dirs[i]);
        wd_dirs[i] = inotify_add_watch(fd, dir_paths[i],
            IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO);
        if (wd_dirs[i] < 0) {
            perror(dir_paths[i]);
        } else {
            printf("Watching directory: %s\n", dir_paths[i]);
        }
    }

    char buf[BUF_LEN] __attribute__ ((aligned(8)));

    while (1) {
        int length = read(fd, buf, BUF_LEN);
        if (length < 0) {
            usleep(250000);
            continue;
        }

        int i = 0;
        while (i < length) {
            struct inotify_event *event = (struct inotify_event *) &buf[i];

            if (event->len > 0) {
                for (int j = 0; j < file_count; ++j) {
                    const char *target = strrchr(watched_files[j], '/')
                                         ? strrchr(watched_files[j], '/') + 1
                                         : watched_files[j];
                    if (strcmp(event->name, target) == 0) {
                        char msg[PATH_MAX + 128];
                        snprintf(msg, sizeof(msg),
                            "File %s was %s",
                            watched_files[j],
                            (event->mask & IN_MODIFY) ? "modified" :
                            (event->mask & IN_CREATE) ? "created" :
                            (event->mask & IN_DELETE) ? "deleted" :
                            (event->mask & IN_MOVED_FROM) ? "moved from" :
                            (event->mask & IN_MOVED_TO) ? "moved to" :
                            "changed");
                        puts(msg);
                        log_event(msg);
                    }
                }
            }

            i += sizeof(struct inotify_event) + event->len;
        }
    }

    close(fd);
    return 0;
}


