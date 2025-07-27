// main_cyon.c

#include <gtk/gtk.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>
#include <string.h>

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
void show_tools(GtkWidget *widget, gpointer data) {
    g_print("Tools selected.\n");
}

// Dummy Security handler
void show_security(GtkWidget *widget, gpointer data) {
    g_print("Security selected.\n");
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    // Create main window
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Cyon");
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 400);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Create vertical box
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // Create menu bar
    GtkWidget *menubar = gtk_menu_bar_new();
    GtkWidget *programs_menu = gtk_menu_new();
    GtkWidget *programs_item = gtk_menu_item_new_with_label("Programs");

    // Create menu items
    GtkWidget *tools_item = gtk_menu_item_new_with_label("Tools");
    GtkWidget *security_item = gtk_menu_item_new_with_label("Security");
    GtkWidget *cli_item = gtk_menu_item_new_with_label("CLI Terminal");

    // Add items to the menu
    gtk_menu_shell_append(GTK_MENU_SHELL(programs_menu), tools_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(programs_menu), security_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(programs_menu), cli_item);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(programs_item), programs_menu);

    // Add menu to the bar
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), programs_item);
    gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 0);

    // Connect signals
    g_signal_connect(tools_item, "activate", G_CALLBACK(show_tools), NULL);
    g_signal_connect(security_item, "activate", G_CALLBACK(show_security), NULL);
    g_signal_connect(cli_item, "activate", G_CALLBACK(launch_cli), NULL);

    // Add a label for aesthetics
    GtkWidget *label = gtk_label_new("Welcome to Cyon!");
    gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 10);

    // Show everything
    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}




