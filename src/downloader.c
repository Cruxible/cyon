//Author: Jonathan Cruxible
//Date Created: jul 27, 2025
#include "../include/downloader.h"

#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>

// Helper function to show a Save File dialog and get the chosen path
// Returns a newly allocated string with the path, or NULL if cancelled.
// Caller must free the returned string.
char* show_save_file_dialog(GtkWindow *parent, const char *default_name) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Save File",
        parent,
        GTK_FILE_CHOOSER_ACTION_SAVE,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Save", GTK_RESPONSE_ACCEPT,
        NULL);

    GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
    gtk_file_chooser_set_do_overwrite_confirmation(chooser, TRUE);

    if (default_name) {
        gtk_file_chooser_set_current_name(chooser, default_name);
    }

    char *filename = NULL;
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        filename = gtk_file_chooser_get_filename(chooser);
    }

    gtk_widget_destroy(dialog);
    return filename;
}

void on_curl_download_clicked(GtkWidget *widget, gpointer data) {
    GtkEntry *entry = GTK_ENTRY(data);
    const gchar *url = gtk_entry_get_text(entry);

    if (g_strcmp0(url, "") == 0) {
        g_print("No URL provided.\n");
        return;
    }

    GtkWindow *parent_window = GTK_WINDOW(gtk_widget_get_toplevel(widget));
    char *save_path = show_save_file_dialog(parent_window, "downloaded_file");

    if (!save_path) {
        g_print("Save dialog canceled.\n");
        return;
    }

    char command[2048];
    snprintf(command, sizeof(command), "curl -L \"%s\" -o \"%s\"", url, save_path);

    g_print("Running: %s\n", command);
    GError *error = NULL;

    if (!g_spawn_command_line_async(command, &error)) {
        g_printerr("Failed to run curl command: %s\n", error->message);
        g_error_free(error);
    }

    g_free(save_path);
}

void on_mp3_download_clicked(GtkWidget *widget, gpointer data) {
    GtkEntry *entry = GTK_ENTRY(data);
    const gchar *url = gtk_entry_get_text(entry);

    if (g_strcmp0(url, "") == 0) {
        g_print("No URL provided.\n");
        return;
    }

    // Get the parent window from the button widget (assuming button is inside your window)
    GtkWindow *parent_window = GTK_WINDOW(gtk_widget_get_toplevel(widget));

    // Show save file dialog with default filename "video.mp3"
    char *save_path = show_save_file_dialog(parent_window, "song.mp3");
    if (!save_path) {
        g_print("Save dialog canceled.\n");
        return;
    }

    // Build the command with the chosen save path
    char command[2048];
    snprintf(command, sizeof(command),
             "yt-dlp -f bestaudio --extract-audio --audio-format mp3 --audio-quality 0 -o '%s' \"%s\"",
             save_path, url);

    g_print("Running: %s\n", command);

    GError *error = NULL;
    if (!g_spawn_command_line_async(command, &error)) {
        g_printerr("Failed to run command: %s\n", error->message);
        g_error_free(error);
    }

    g_free(save_path);
}

void on_download_clicked(GtkWidget *widget, gpointer data) {
    GtkEntry *entry = GTK_ENTRY(data);
    const gchar *url = gtk_entry_get_text(entry);

    if (g_strcmp0(url, "") == 0) {
        g_print("No URL provided.\n");
        return;
    }

    GtkWindow *parent_window = GTK_WINDOW(gtk_widget_get_toplevel(widget));

    // Default filename with mp4 extension
    char *save_path = show_save_file_dialog(parent_window, "video.mp4");
    if (!save_path) {
        g_print("Save dialog canceled.\n");
        return;
    }

    char command[2048];
    snprintf(command, sizeof(command),
             "yt-dlp -f bestvideo+bestaudio --merge-output-format mp4 -o '%s' \"%s\"",
             save_path, url);

    g_print("Running: %s\n", command);

    GError *error = NULL;
    if (!g_spawn_command_line_async(command, &error)) {
        g_printerr("Failed to run command: %s\n", error->message);
        g_error_free(error);
    }

    g_free(save_path);
}

void show_downloader_window(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "YouTube Downloader");
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 100);  // Larger window
    gtk_window_set_resizable(GTK_WINDOW(window), TRUE);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(box), 10);

    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Paste YouTube link here...");
    gtk_box_pack_start(GTK_BOX(box), entry, TRUE, TRUE, 0);

    GtkWidget *button = gtk_button_new_with_label("Download MP4 to Desktop");
    gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
    g_signal_connect(button, "clicked", G_CALLBACK(on_download_clicked), entry);

    GtkWidget *button1 = gtk_button_new_with_label("Download MP3 to Desktop");
    gtk_box_pack_start(GTK_BOX(box), button1, FALSE, FALSE, 0);
    g_signal_connect(button1, "clicked", G_CALLBACK(on_mp3_download_clicked), entry);

    GtkWidget *button2 = gtk_button_new_with_label("Download File via URL");
    gtk_box_pack_start(GTK_BOX(box), button2, FALSE, FALSE, 0);
    g_signal_connect(button2, "clicked", G_CALLBACK(on_curl_download_clicked), entry);

    gtk_container_add(GTK_CONTAINER(window), box);
    gtk_widget_show_all(window);
    gtk_main();
}
