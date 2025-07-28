#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include "../include/downloader.h"

// Function to launch cyon_cli as a separate process
void run_cyon_cli() {
    const char *terminals[] = {
        "mate-terminal",
        "gnome-terminal",
        "xfce4-terminal",
        "xterm",
        NULL
    };

    for (int i = 0; terminals[i] != NULL; ++i) {
        pid_t pid = fork();

        if (pid == 0) {
            // Child process
            setsid();  // Optional: new session
            execlp(terminals[i], terminals[i], "-e", "./cyon_cli", (char *)NULL);
            _exit(1);  // exec failed
        } else if (pid > 0) {
            int status;
            waitpid(pid, &status, 0);

            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                return;  // Success
            }
        } else {
            perror("fork failed");
        }
    }

    fprintf(stderr, "Could not launch cyon_cli in any terminal.\n");
}

// Callback for menu item to open the downloader window
static void on_open_downloader(GtkWidget *widget, gpointer data) {
    show_downloader_window();  // Defined in downloader.c
}

// Callback to run the CLI tool
static void on_run_cli(GtkWidget *widget, gpointer data) {
    run_cyon_cli();  // Calls the real launcher
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    // Create main window
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Cyon");
    gtk_window_set_default_size(GTK_WINDOW(win), 500, 500);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Main container
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
    gtk_container_add(GTK_CONTAINER(win), vbox);

    // Create menu bar
    GtkWidget *menu_bar = gtk_menu_bar_new();

    // "Tools" menu
    GtkWidget *tools_menu = gtk_menu_new();
    GtkWidget *tools_item = gtk_menu_item_new_with_label("Tools");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(tools_item), tools_menu);

    // "Open Downloader" menu item
    GtkWidget *open_item = gtk_menu_item_new_with_label("Open Downloader");
    g_signal_connect(open_item, "activate", G_CALLBACK(on_open_downloader), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(tools_menu), open_item);

    // "Open Cyon CLI" menu item
    GtkWidget *open_item_cli = gtk_menu_item_new_with_label("Open Cyon CLI");
    g_signal_connect(open_item_cli, "activate", G_CALLBACK(on_run_cli), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(tools_menu), open_item_cli);

    // Add tools menu to menu bar
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), tools_item);

    // Add menu bar to the container
    gtk_box_pack_start(GTK_BOX(vbox), menu_bar, FALSE, FALSE, 0);

    // Add a label for aesthetics
    GtkWidget *label = gtk_label_new("Welcome to Cyon!");
    gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 10);

    // Show everything
    gtk_widget_show_all(win);
    gtk_main();

    return 0;
}





