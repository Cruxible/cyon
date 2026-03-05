// Author: Jonathan Cruxible
// Date Created: Jul 27, 2025
#include "../include/downloader.h"

#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>

/* ------------------------------------------------------------------ */
/*  CYON PS1 CSS                                                        */
/* ------------------------------------------------------------------ */
static const char *CYON_CSS =
    "window, .background { background-color: #0a0a0f; }"
    "label {"
    "  color: #00cc77; font-family: monospace;"
    "  font-size: 11px; letter-spacing: 1px;"
    "}"
    ".title-label {"
    "  color: #00ff99; font-family: monospace;"
    "  font-size: 13px; font-weight: bold; letter-spacing: 4px;"
    "}"
    "button {"
    "  background-color: #0d0d15; color: #00cc77;"
    "  font-family: monospace; font-size: 12px;"
    "  border: 1px solid #1a2a20; border-radius: 0px; padding: 6px 18px;"
    "}"
    "button:hover {"
    "  background-color: #003322; color: #00ff99;"
    "  border-color: #00ff99; box-shadow: 0 0 4px #00ff99;"
    "}"
    "entry {"
    "  background-color: #0d0d15; color: #00ff99;"
    "  font-family: monospace; font-size: 12px;"
    "  border: 1px solid #1a2a20; border-radius: 0px; padding: 4px 8px;"
    "  caret-color: #00ff99;"
    "}"
    "textview, textview text {"
    "  background-color: #050508; color: #00cc77;"
    "  font-family: monospace; font-size: 11px;"
    "}"
    ".marquee-label {"
    "  color: #00ff99; font-family: monospace;"
    "  font-size: 12px; letter-spacing: 2px;"
    "  background-color: #050508;"
    "}"
    "separator { background-color: #1a2a20; }"
    "scrolledwindow { border: 1px solid #1a2a20; }";

/* ------------------------------------------------------------------ */
/*  Meme marquee messages                                               */
/* ------------------------------------------------------------------ */
static const char *MARQUEE_MSGS[] = {
    "▸ YOINKING FROM THE INTERNET...",
    "▸ BRIBING THE SERVER WITH COOKIES...",
    "▸ DOWNLOADING MORE RAM...",
    "▸ ASKING NICELY...",
    "▸ PACKETS INBOUND. DO NOT DISTURB.",
    "▸ SKILL ISSUE? NOT TODAY.",
    "▸ BYTES GO BRRR...",
    "▸ NEGOTIATING WITH CLOUDFLARE...",
    "▸ CONVERTING VIBES TO DATA...",
    "▸ THIS IS FINE. EVERYTHING IS FINE.",
    "▸ ENGAGING TURBO MODE...",
    "▸ FEEDING THE HAMSTERS...",
    "▸ NO CAP, FULL BANDWIDTH...",
    "▸ BUFFER? WE DON'T DO THAT HERE.",
    "▸ SPEED RUN ANY%...",
    "▸ THE BITS ARE TRAVELLING. BE PATIENT.",
    "▸ ROUTING THROUGH THE SHADOW REALM...",
    "▸ W RIZZ. FILE INCOMING.",
    "▸ TOUCHING GRASS LATER. DOWNLOADING NOW.",
    "▸ PACKET LOSS IS A MYTH.",
};
static const int MARQUEE_COUNT = 20;

/* ------------------------------------------------------------------ */
/*  Progress window state                                               */
/* ------------------------------------------------------------------ */
typedef struct {
    GtkWidget     *window;
    GtkWidget     *marquee_label;
    GtkWidget     *textview;
    GtkWidget     *status_label;
    GtkTextBuffer *buffer;

    /* marquee scroll state */
    char  marquee_buf[256];
    int   marquee_offset;
    int   marquee_msg_idx;
    guint marquee_timer;

    /* pipe watching */
    GIOChannel *stdout_chan;
    GIOChannel *stderr_chan;
    guint       stdout_watch;
    guint       stderr_watch;
} ProgressWin;

/* ------------------------------------------------------------------ */
/*  Marquee animation                                                   */
/* ------------------------------------------------------------------ */
#define MARQUEE_WIDTH 52

static void load_next_marquee_msg(ProgressWin *pw) {
    pw->marquee_msg_idx = (pw->marquee_msg_idx + 1) % MARQUEE_COUNT;
    snprintf(pw->marquee_buf, sizeof(pw->marquee_buf),
             "%-*s%s",
             MARQUEE_WIDTH, "",
             MARQUEE_MSGS[pw->marquee_msg_idx]);
    pw->marquee_offset = 0;
}

static gboolean tick_marquee(gpointer data) {
    ProgressWin *pw = (ProgressWin *)data;
    if (!GTK_IS_WIDGET(pw->marquee_label)) return G_SOURCE_REMOVE;

    int len = (int)strlen(pw->marquee_buf);
    if (pw->marquee_offset >= len)
        load_next_marquee_msg(pw);

    char view[MARQUEE_WIDTH + 1];
    int remaining = len - pw->marquee_offset;
    int copy = remaining < MARQUEE_WIDTH ? remaining : MARQUEE_WIDTH;
    memcpy(view, pw->marquee_buf + pw->marquee_offset, copy);
    memset(view + copy, ' ', MARQUEE_WIDTH - copy);
    view[MARQUEE_WIDTH] = '\0';

    gtk_label_set_text(GTK_LABEL(pw->marquee_label), view);
    pw->marquee_offset += 2;
    return G_SOURCE_CONTINUE;
}

/* ------------------------------------------------------------------ */
/*  Pipe I/O                                                            */
/* ------------------------------------------------------------------ */
static void append_text(ProgressWin *pw, const char *text) {
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(pw->buffer, &end);
    gtk_text_buffer_insert(pw->buffer, &end, text, -1);
    GtkTextMark *mark = gtk_text_buffer_get_insert(pw->buffer);
    gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(pw->textview), mark);
}

static gboolean on_pipe_data(GIOChannel *chan, GIOCondition cond, gpointer data) {
    ProgressWin *pw = (ProgressWin *)data;

    if (cond & (G_IO_HUP | G_IO_ERR)) {
        if (GTK_IS_WIDGET(pw->status_label))
            gtk_label_set_text(GTK_LABEL(pw->status_label),
                               "▸ TRANSFER COMPLETE. GO TOUCH GRASS.");
        if (pw->marquee_timer) {
            g_source_remove(pw->marquee_timer);
            pw->marquee_timer = 0;
        }
        if (GTK_IS_WIDGET(pw->marquee_label))
            gtk_label_set_text(GTK_LABEL(pw->marquee_label),
                               "          ▸▸▸  MISSION ACCOMPLISHED  ◀◀◀");
        return G_SOURCE_REMOVE;
    }

    gchar  *line = NULL;
    gsize   len  = 0;
    GError *err  = NULL;
    GIOStatus status = g_io_channel_read_line(chan, &line, &len, NULL, &err);
    if (status == G_IO_STATUS_NORMAL && line) {
        append_text(pw, line);
        g_free(line);
    }
    if (err) g_error_free(err);
    return G_SOURCE_CONTINUE;
}

/* ------------------------------------------------------------------ */
/*  Build the progress window                                           */
/* ------------------------------------------------------------------ */
static ProgressWin *create_progress_window(GtkWindow *parent, const char *title) {
    ProgressWin *pw = g_new0(ProgressWin, 1);

    pw->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(pw->window), title);
    gtk_window_set_default_size(GTK_WINDOW(pw->window), 580, 360);
    gtk_window_set_resizable(GTK_WINDOW(pw->window), TRUE);
    if (parent)
        gtk_window_set_transient_for(GTK_WINDOW(pw->window), parent);

    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(outer), 10);
    gtk_container_add(GTK_CONTAINER(pw->window), outer);

    /* title */
    GtkWidget *title_lbl = gtk_label_new(title);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(title_lbl), "title-label");
    gtk_widget_set_halign(title_lbl, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(outer), title_lbl, FALSE, FALSE, 6);

    GtkWidget *sep1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(outer), sep1, FALSE, FALSE, 4);

    /* marquee */
    pw->marquee_label = gtk_label_new("");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(pw->marquee_label), "marquee-label");
    gtk_widget_set_halign(pw->marquee_label, GTK_ALIGN_FILL);
    gtk_label_set_selectable(GTK_LABEL(pw->marquee_label), FALSE);
    gtk_box_pack_start(GTK_BOX(outer), pw->marquee_label, FALSE, FALSE, 4);

    GtkWidget *sep2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(outer), sep2, FALSE, FALSE, 4);

    /* scrolled terminal output */
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_box_pack_start(GTK_BOX(outer), scroll, TRUE, TRUE, 4);

    pw->textview = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(pw->textview), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(pw->textview), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(pw->textview), GTK_WRAP_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(pw->textview), 6);
    pw->buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(pw->textview));
    gtk_container_add(GTK_CONTAINER(scroll), pw->textview);

    GtkWidget *sep3 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(outer), sep3, FALSE, FALSE, 4);

    /* status line */
    pw->status_label = gtk_label_new("▸ INITIATING DOWNLOAD SEQUENCE...");
    gtk_widget_set_halign(pw->status_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(outer), pw->status_label, FALSE, FALSE, 4);

    gtk_widget_show_all(pw->window);

    /* kick off marquee */
    snprintf(pw->marquee_buf, sizeof(pw->marquee_buf),
             "%-*s%s", MARQUEE_WIDTH, "", MARQUEE_MSGS[0]);
    pw->marquee_offset  = 0;
    pw->marquee_msg_idx = 0;
    pw->marquee_timer   = g_timeout_add(80, tick_marquee, pw);

    return pw;
}

/* ------------------------------------------------------------------ */
/*  Launch command and wire up pipes                                    */
/* ------------------------------------------------------------------ */
static void launch_with_progress(const char *command,
                                  GtkWindow  *parent,
                                  const char *win_title) {
    const gchar *argv[] = { "bash", "-c", command, NULL };
    GPid  child_pid;
    gint  stdout_fd, stderr_fd;
    GError *error = NULL;

    gboolean ok = g_spawn_async_with_pipes(
        NULL,
        (gchar **)argv,
        NULL,
        G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
        NULL, NULL,
        &child_pid,
        NULL,
        &stdout_fd,
        &stderr_fd,
        &error);

    if (!ok) {
        GtkWidget *dlg = gtk_message_dialog_new(
            parent, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
            GTK_BUTTONS_CLOSE, "Failed to launch: %s", error->message);
        gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_widget_destroy(dlg);
        g_error_free(error);
        return;
    }

    ProgressWin *pw = create_progress_window(parent, win_title);

    pw->stdout_chan = g_io_channel_unix_new(stdout_fd);
    g_io_channel_set_flags(pw->stdout_chan, G_IO_FLAG_NONBLOCK, NULL);
    g_io_channel_set_encoding(pw->stdout_chan, NULL, NULL);
    pw->stdout_watch = g_io_add_watch(pw->stdout_chan,
                                      G_IO_IN | G_IO_HUP | G_IO_ERR,
                                      on_pipe_data, pw);

    pw->stderr_chan = g_io_channel_unix_new(stderr_fd);
    g_io_channel_set_flags(pw->stderr_chan, G_IO_FLAG_NONBLOCK, NULL);
    g_io_channel_set_encoding(pw->stderr_chan, NULL, NULL);
    pw->stderr_watch = g_io_add_watch(pw->stderr_chan,
                                      G_IO_IN | G_IO_HUP | G_IO_ERR,
                                      on_pipe_data, pw);

    g_child_watch_add(child_pid, (GChildWatchFunc)g_spawn_close_pid, NULL);
}

/* ------------------------------------------------------------------ */
/*  Utility dialogs                                                     */
/* ------------------------------------------------------------------ */
static void show_error_dialog(GtkWindow *parent, const char *message) {
    GtkWidget *dialog = gtk_message_dialog_new(
        parent, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", message);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static char *show_save_file_dialog(GtkWindow *parent, const char *default_name) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Save File", parent, GTK_FILE_CHOOSER_ACTION_SAVE,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Save",   GTK_RESPONSE_ACCEPT,
        NULL);
    GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
    gtk_file_chooser_set_do_overwrite_confirmation(chooser, TRUE);
    if (default_name)
        gtk_file_chooser_set_current_name(chooser, default_name);
    char *filename = NULL;
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
        filename = gtk_file_chooser_get_filename(chooser);
    gtk_widget_destroy(dialog);
    return filename;
}

/* ------------------------------------------------------------------ */
/*  Button handlers                                                     */
/* ------------------------------------------------------------------ */
static void on_curl_download_clicked(GtkWidget *widget, gpointer data) {
    GtkEntry    *entry  = GTK_ENTRY(data);
    const gchar *url    = gtk_entry_get_text(entry);
    GtkWindow   *parent = GTK_WINDOW(gtk_widget_get_toplevel(widget));
    if (g_strcmp0(url, "") == 0) { show_error_dialog(parent, "No URL provided."); return; }

    char *save_path = show_save_file_dialog(parent, "downloaded_file");
    if (!save_path) return;

    char command[4096];
    snprintf(command, sizeof(command),
             "curl -L --progress-bar \"%s\" -o \"%s\" 2>&1",
             url, save_path);
    launch_with_progress(command, parent, "CYON // CURL DOWNLOAD");
    g_free(save_path);
}

static void on_mp3_download_clicked(GtkWidget *widget, gpointer data) {
    GtkEntry    *entry  = GTK_ENTRY(data);
    const gchar *url    = gtk_entry_get_text(entry);
    GtkWindow   *parent = GTK_WINDOW(gtk_widget_get_toplevel(widget));
    if (g_strcmp0(url, "") == 0) { show_error_dialog(parent, "No URL provided."); return; }

    char *save_path = show_save_file_dialog(parent, "song.mp3");
    if (!save_path) return;

    char command[4096];
    snprintf(command, sizeof(command),
             "yt-dlp -f bestaudio --extract-audio --audio-format mp3 "
             "--audio-quality 0 --newline -o \"%s\" \"%s\" 2>&1",
             save_path, url);
    launch_with_progress(command, parent, "CYON // MP3 DOWNLOAD");
    g_free(save_path);
}

static void on_download_clicked(GtkWidget *widget, gpointer data) {
    GtkEntry    *entry  = GTK_ENTRY(data);
    const gchar *url    = gtk_entry_get_text(entry);
    GtkWindow   *parent = GTK_WINDOW(gtk_widget_get_toplevel(widget));
    if (g_strcmp0(url, "") == 0) { show_error_dialog(parent, "No URL provided."); return; }

    char *save_path = show_save_file_dialog(parent, "video.mp4");
    if (!save_path) return;

    char command[4096];
    snprintf(command, sizeof(command),
             "yt-dlp -f bestvideo+bestaudio --merge-output-format mp4 "
             "--newline -o \"%s\" \"%s\" 2>&1",
             save_path, url);
    launch_with_progress(command, parent, "CYON // MP4 DOWNLOAD");
    g_free(save_path);
}

/* ------------------------------------------------------------------ */
/*  Main downloader window                                              */
/* ------------------------------------------------------------------ */
void show_downloader_window(void) {
    GtkCssProvider *prov = gtk_css_provider_new();
    gtk_css_provider_load_from_data(prov, CYON_CSS, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(prov),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "CYON // DOWNLOADER");
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 200);
    gtk_window_set_resizable(GTK_WINDOW(window), TRUE);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_widget_destroy), window);

    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(outer), 12);
    gtk_container_add(GTK_CONTAINER(window), outer);

    GtkWidget *title = gtk_label_new("▸ CYON // DOWNLOADER");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "title-label");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(outer), title, FALSE, FALSE, 6);

    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(outer), sep, FALSE, FALSE, 6);

    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "▸ PASTE LINK HERE...");
    gtk_box_pack_start(GTK_BOX(outer), entry, FALSE, FALSE, 6);

    GtkWidget *sep2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(outer), sep2, FALSE, FALSE, 6);

    GtkWidget *btn_mp4 = gtk_button_new_with_label("▸ DOWNLOAD MP4");
    gtk_box_pack_start(GTK_BOX(outer), btn_mp4, FALSE, FALSE, 3);
    g_signal_connect(btn_mp4, "clicked", G_CALLBACK(on_download_clicked), entry);

    GtkWidget *btn_mp3 = gtk_button_new_with_label("▸ DOWNLOAD MP3");
    gtk_box_pack_start(GTK_BOX(outer), btn_mp3, FALSE, FALSE, 3);
    g_signal_connect(btn_mp3, "clicked", G_CALLBACK(on_mp3_download_clicked), entry);

    GtkWidget *btn_curl = gtk_button_new_with_label("▸ DOWNLOAD FILE VIA URL");
    gtk_box_pack_start(GTK_BOX(outer), btn_curl, FALSE, FALSE, 3);
    g_signal_connect(btn_curl, "clicked", G_CALLBACK(on_curl_download_clicked), entry);

    gtk_widget_show_all(window);
}
