#include <gtk/gtk.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>
#include <string.h>
#include "downloader.h"  // Ensure this header defines show_downloader_window()

// Launch cyon_cli in a terminal
void launch_cli(GtkWidget *widget, gpointer data) {
    const char *terminals[] = {
        "mate-terminal", "x-terminal-emulator", "gnome-terminal",
        "xfce4-terminal", "lxterminal", "konsole", "xterm"
    };

    char full_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", full_path, sizeof(full_path) - 1);
    if (len == -1) {
        perror("readlink");
        return;
    }

    full_path[len] = '\0';
    char *dir = dirname(full_path);

    char cli_path[PATH_MAX];
    snprintf(cli_path, sizeof(cli_path), "%s/cyon_cli", dir);

    for (int i = 0; i < sizeof(terminals)/sizeof(terminals[0]); i++) {
        char cmd[PATH_MAX + 64];
        snprintf(cmd, sizeof(cmd), "%s -e '%s'", terminals[i], cli_path);
        if (system(cmd) == 0) return;
    }

    g_print("No compatible terminal found.\n");
}

// Dummy Tools handler
void tool_a(GtkWidget *widget, gpointer data) {
    g_print("Tool A selected.\n");
}

void tool_b(GtkWidget *widget, gpointer data) {
    g_print("Tool B selected.\n");
}

// Dummy Security handler
void firewall_sec(GtkWidget *widget, gpointer data) {
    g_print("Firewall selected.\n");
}

void scanner_sec(GtkWidget *widget, gpointer data) {
    g_print("Scanner selected.\n");
}

// Downloader handler
void open_downloader(GtkWidget *widget, gpointer data) {
    show_downloader_window();  // Function from downloader.c
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    // Create main window
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Cyon");
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 400);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Vertical layout
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // Menu bar
    GtkWidget *menubar = gtk_menu_bar_new();

    // === Programs Menu ===
    GtkWidget *programs_menu = gtk_menu_new();
    GtkWidget *programs_item = gtk_menu_item_new_with_label("Programs");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(programs_item), programs_menu);

    // CLI Terminal
    GtkWidget *cli_item = gtk_menu_item_new_with_label("CLI Terminal");
    gtk_menu_shell_append(GTK_MENU_SHELL(programs_menu), cli_item);
    g_signal_connect(cli_item, "activate", G_CALLBACK(launch_cli), NULL);

    // Downloader
    GtkWidget *downloader_item = gtk_menu_item_new_with_label("Downloader");
    gtk_menu_shell_append(GTK_MENU_SHELL(programs_menu), downloader_item);
    g_signal_connect(downloader_item, "activate", G_CALLBACK(open_downloader), NULL);

    // === Tools Submenu ===
    GtkWidget *tools_item = gtk_menu_item_new_with_label("Tools");
    GtkWidget *tools_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(tools_item), tools_menu);

    GtkWidget *tool_subitem1 = gtk_menu_item_new_with_label("Tool A");
    GtkWidget *tool_subitem2 = gtk_menu_item_new_with_label("Tool B");
    gtk_menu_shell_append(GTK_MENU_SHELL(tools_menu), tool_subitem1);
    gtk_menu_shell_append(GTK_MENU_SHELL(tools_menu), tool_subitem2);
    gtk_menu_shell_append(GTK_MENU_SHELL(programs_menu), tools_item);

    g_signal_connect(tool_subitem1, "activate", G_CALLBACK(tool_a), NULL);
    g_signal_connect(tool_subitem2, "activate", G_CALLBACK(tool_b), NULL);

    // === Security Submenu ===
    GtkWidget *security_item = gtk_menu_item_new_with_label("Security");
    GtkWidget *security_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(security_item), security_menu);

    GtkWidget *security_subitem1 = gtk_menu_item_new_with_label("Firewall");
    GtkWidget *security_subitem2 = gtk_menu_item_new_with_label("Scanner");
    gtk_menu_shell_append(GTK_MENU_SHELL(security_menu), security_subitem1);
    gtk_menu_shell_append(GTK_MENU_SHELL(security_menu), security_subitem2);
    gtk_menu_shell_append(GTK_MENU_SHELL(programs_menu), security_item);

    g_signal_connect(security_subitem1, "activate", G_CALLBACK(firewall_sec), NULL);
    g_signal_connect(security_subitem2, "activate", G_CALLBACK(scanner_sec), NULL);

    // Attach menubar
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), programs_item);
    gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 0);

    // Welcome label
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), "<span font='20'>Welcome to Cyon!</span>");
    gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 10);

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}
