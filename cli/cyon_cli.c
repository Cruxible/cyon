#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>

#define MAX_CMD_LEN 1024

// Function to print the prompt
void print_prompt() {
    // Get username
    struct passwd *pw = getpwuid(getuid());
    const char *username = pw ? pw->pw_name : "user";

    // Get current working directory
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd() error");
        strcpy(cwd, "?");
    }

    // Print prompt
    printf(" \033[96m┌░\033[37m%s\033[96m░\n └░\033[37m%s\033[96m░\033[37m ", cwd, username);
    fflush(stdout);
}

int main() {
    printf("\033[94m Cyon CLI Tool - Running standalone.\033[37m\n\n tools\n security\n exit\n\n");

    char command[MAX_CMD_LEN];

    while (1) {
        print_prompt();

        // Read user input
        if (fgets(command, sizeof(command), stdin) == NULL) {
            printf("\n");
            break; // handle Ctrl+D
        }

        // Remove trailing newline
        command[strcspn(command, "\n")] = 0;

        if (strcmp(command, "exit") == 0) {
            break;
        }

        if (strcmp(command, "tools") == 0) {
            printf(" Tools menu\n");
            char command[MAX_CMD_LEN];
            while (1) {
                print_prompt();
                // Read user input
                if (fgets(command, sizeof(command), stdin) == NULL) {
                    printf("\n");
                    break; // handle Ctrl+D
                }
                // Remove trailing newline
                command[strcspn(command, "\n")] = 0;

                if (strcmp(command, "exit") == 0) {
                    printf(" Exiting Tools Menu.\n");
                    break;
                }
            }
            // You can call print_prompt() here again if you want
        }

        if (strcmp(command, "security") == 0) {
            printf(" Security menu\n");
            char command[MAX_CMD_LEN];
            while (1) {
                print_prompt();
                // Read user input
                if (fgets(command, sizeof(command), stdin) == NULL) {
                    printf("\n");
                    break; // handle Ctrl+D
                }
                // Remove trailing newline
                command[strcspn(command, "\n")] = 0;

                if (strcmp(command, "exit") == 0) {
                    printf("Exiting Security Menu.\n");
                    break;
                }
            }
        }
    }

    printf("Goodbye!\n");
    return 0;
}

