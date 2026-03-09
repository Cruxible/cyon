#include <gtk/gtk.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <libgen.h>
#include <limits.h>
#include <fcntl.h>
#include "../include/downloader.h"
static void ui_log(const char *fmt, ...);
/* Paths resolved at runtime — source install */
#define CYON_PATH_MAX (PATH_MAX * 2)
static char BOT_SCRIPT[CYON_PATH_MAX];
static char LOCAL_BOT_SCRIPT[CYON_PATH_MAX];
static char SHELL_SCRIPT[CYON_PATH_MAX];
static char VENV_PYTHON[CYON_PATH_MAX];

static void resolve_paths(void) {
    const char *cyon_home = getenv("CYON_HOME");
    const char *home      = getenv("HOME");
    const char *base      = cyon_home ? cyon_home : (home ? ({ static char b[PATH_MAX]; snprintf(b, sizeof(b), "%s/cyon", home); b; }) : "/cyon");

    snprintf(BOT_SCRIPT,       sizeof(BOT_SCRIPT),       "%s/cyon_bot.py",    base);
    snprintf(LOCAL_BOT_SCRIPT, sizeof(LOCAL_BOT_SCRIPT), "%s/cyon_local.py",  base);
    snprintf(SHELL_SCRIPT,     sizeof(SHELL_SCRIPT),     "%s/cyon_shell.py",  base);

    /* pyra_env always lives in ~/pyra_env regardless of install method */
    if (home)
        snprintf(VENV_PYTHON, sizeof(VENV_PYTHON), "%s/pyra_env/bin/python3", home);
    else
        snprintf(VENV_PYTHON, sizeof(VENV_PYTHON), "/usr/bin/python3");
}
/* ── Global process handles ─────────────────────────────────────────────── */
static GPid ollama_pid = 0, bot_pid = 0, local_pid = 0, shell_pid = 0;
static int  ollama_fd = -1, bot_fd = -1, local_stdout_fd = -1, local_stdin_fd = -1, shell_stdin_fd = -1, shell_stdout_fd = -1;
/* ── Widget refs ─────────────────────────────────────────────────────────── */
static GtkWidget     *ollama_status_lbl, *bot_status_lbl, *local_status_lbl;
static GtkTextBuffer *log_buffer;
static GtkWidget     *log_view;
static GtkTextTag    *shell_tag = NULL;
static GtkTextTag    *default_tag = NULL;

/* ── Marquee ─────────────────────────────────────────────────────────────── */
static GtkWidget *marquee_label = NULL;
static char       marquee_buf[256];
static int        marquee_offset   = 0;
static int        marquee_msg_idx  = 0;
#define MAIN_MARQUEE_WIDTH 54
static const char *MAIN_MARQUEE_MSGS[] = {
    "▸ <Commands> /shutdown  — INITIATING SELF-DESTRUCT SEQUENCE...",
    "▸ /bye       — GRACEFUL EXIT. CYON OUT.",
    "▸ /clear     — MEMORY WIPED. WHAT WERE WE TALKING ABOUT?",
    "▸ /quiet     — CYON GOING DARK. NO MORE UNSOLICITED OPINIONS.",
    "▸ /loud      — CYON HAS OPINIONS AGAIN. YOU ASKED FOR THIS.",
    "▸ /pyra      — SUMMONING THE PYRA TOOL...",
    "▸ /cyon_cli  — DROPPING INTO THE CLI UNDERWORLD...",
    "▸ /term      — SPAWNING A TERMINAL. GODSPEED.",
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
    "▸ CYON ONLINE. REALITY OPTIONAL.",
    "▸ ALL SYSTEMS... PROBABLY FINE.",
    "▸ ESTABLISHING LINK TO THE VOID...",
    "▸ DO NOT LOOK DIRECTLY AT THE LOG.",
    "▸ THE HORRORS PERSIST, BUT SO DO I."
};
static const int MAIN_MARQUEE_COUNT = 24;

static void main_load_next_marquee(void) {
    marquee_msg_idx = (marquee_msg_idx + 1) % MAIN_MARQUEE_COUNT;
    snprintf(marquee_buf, sizeof(marquee_buf),
             "%-*s%s", MAIN_MARQUEE_WIDTH, "", MAIN_MARQUEE_MSGS[marquee_msg_idx]);
    marquee_offset = 0;
}

static gboolean main_tick_marquee(gpointer data) {
    if (!marquee_label || !GTK_IS_WIDGET(marquee_label)) return G_SOURCE_REMOVE;
    int len = (int)strlen(marquee_buf);
    if (marquee_offset >= len) main_load_next_marquee();
    char view[MAIN_MARQUEE_WIDTH + 1];
    int remaining = len - marquee_offset;
    int copy = remaining < MAIN_MARQUEE_WIDTH ? remaining : MAIN_MARQUEE_WIDTH;
    memcpy(view, marquee_buf + marquee_offset, copy);
    memset(view + copy, ' ', MAIN_MARQUEE_WIDTH - copy);
    view[MAIN_MARQUEE_WIDTH] = '\0';
    gtk_label_set_text(GTK_LABEL(marquee_label), view);
    marquee_offset += 1;
    return G_SOURCE_CONTINUE;
}
/* ── CSS ─────────────────────────────────────────────────────────────────── */
static const gchar *CSS =
    "window { background-color: #0a0a0f; }"
    "menubar { background-color: #0d0d15; border-bottom: 1px solid #1a2a20; }"
    ".title-label { color: #00ff99; font-family: monospace; font-size: 18px; font-weight: bold; letter-spacing: 4px; }"
    ".status-label { font-family: monospace; font-size: 12px; padding: 2px 8px; }"
    ".status-off { color: #ff3355; } .status-on { color: #00ff99; }"
    
    /* Green Start Buttons */
    ".btn-start { background-color: #003322; color: #00ff99; border: 1px solid #00ff99; font-family: monospace; padding: 6px 18px; }"
    
    /* Amber Stop Buttons */
    ".btn-stop { background-color: #221100; color: #E8A020; border: 1px solid #E8A020; font-family: monospace; padding: 6px 18px; }"
    ".btn-stop:hover { background-color: #331a00; }"
    
    /* BIG TEXT UPDATES */
    ".log-view { background-color: #05050a; color: #00cc77; font-family: 'Monospace', 'Courier New', monospace; font-size: 20px; }"
    "entry { background-color: #05050a; color: #00ff99; border: 1px solid #00ff99; font-family: monospace; font-size: 20px; padding: 10px; }"
    ".main-marquee { color: #00ff99; font-family: monospace; font-size: 16px; letter-spacing: 2px; background-color: #050508; padding: 2px 6px; }";

/* ── Menu Callbacks ─────────────────────────────────────────────────────── */
void launch_cli(GtkWidget *widget, gpointer data) {
    const char *terminals[] = {
        "mate-terminal", "x-terminal-emulator", "gnome-terminal",
        "xfce4-terminal", "lxterminal", "konsole", "xterm"
    };
    char full_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", full_path, sizeof(full_path) - 1);
    if (len == -1) { perror("readlink"); return; }
    full_path[len] = '\0';
    char *dir = dirname(full_path);
    char cli_path[PATH_MAX];
    snprintf(cli_path, sizeof(cli_path), "%s/cyon_cli", dir);
    for (int i = 0; i < (int)(sizeof(terminals)/sizeof(terminals[0])); i++) {
        char cmd[PATH_MAX + 64];
        snprintf(cmd, sizeof(cmd), "%s -e '%s'", terminals[i], cli_path);
        if (system(cmd) == 0) return;
    }
    g_print("No compatible terminal found.\n");
}

void launch_pyra(GtkWidget *widget, gpointer data) {
    char pyra_path[PATH_MAX];
    const char *home = getenv("HOME");
    const char *cyon_home = getenv("CYON_HOME");

    if (!home) { g_print("Could not determine HOME directory.\n"); return; }

    // Source install: fall back to ~/cyon/pyra_tool/pyra_toolz
    if (cyon_home) {
        snprintf(pyra_path, sizeof(pyra_path), "%s/pyra_tool/pyra_toolz", cyon_home);
    } else {
        snprintf(pyra_path, sizeof(pyra_path), "%s/cyon/pyra_tool/pyra_toolz", home);
    }

    const char *terminals[] = {
        "mate-terminal", "x-terminal-emulator", "gnome-terminal",
        "xfce4-terminal", "lxterminal", "konsole", "xterm"
    };
    for (int i = 0; i < (int)(sizeof(terminals)/sizeof(terminals[0])); i++) {
        char cmd[PATH_MAX + 64];
        snprintf(cmd, sizeof(cmd), "%s -e '%s'", terminals[i], pyra_path);
        if (system(cmd) == 0) return;
    }
    g_print("No compatible terminal found to launch pyra.\n");
}

void launch_repeater(GtkWidget *widget, gpointer data) {
    const char *home      = getenv("HOME");
    const char *cyon_home = getenv("CYON_HOME");
    if (!home) { g_print("Could not determine HOME directory.\n"); return; }
    const char *base = cyon_home ? cyon_home : ({ static char b[PATH_MAX]; snprintf(b, sizeof(b), "%s/cyon", home); b; });
    char script[CYON_PATH_MAX];
    snprintf(script, sizeof(script), "%s/pyra_lib/pyra_repeater.py", base);
    const char *terminals[] = {
        "mate-terminal", "x-terminal-emulator", "gnome-terminal",
        "xfce4-terminal", "lxterminal", "konsole", "xterm"
    };
    for (int i = 0; i < (int)(sizeof(terminals)/sizeof(terminals[0])); i++) {
        char cmd[CYON_PATH_MAX * 4];
        snprintf(cmd, sizeof(cmd), "%s -e '%s %s'", terminals[i], VENV_PYTHON, script);
        if (system(cmd) == 0) return;
    }
    g_print("No compatible terminal found to launch repeater.\n");
}

/* Helper macro for launching gtk_lib Python scripts */
#define LAUNCH_PYLIB(script_name) \
    do { \
        const char *_home = getenv("HOME"); \
        const char *_ch   = getenv("CYON_HOME"); \
        if (!_home) return; \
        const char *_base = _ch ? _ch : ({ static char _b[CYON_PATH_MAX]; snprintf(_b, sizeof(_b), "%s/cyon", _home); _b; }); \
        char _cmd[CYON_PATH_MAX * 2]; \
        snprintf(_cmd, sizeof(_cmd), "python3 %s/pyra_lib/gtk_lib/%s &", _base, script_name); \
        system(_cmd); \
    } while(0)

void open_gtk_convert(GtkWidget *widget, gpointer data)  { LAUNCH_PYLIB("gtk_convert.py"); }
void create_tarfile(GtkWidget *widget, gpointer data)     { LAUNCH_PYLIB("tarmaker_gtk3.py"); }
void open_pyra_notes(GtkWidget *widget, gpointer data)    { LAUNCH_PYLIB("pyra_notes.py"); }
void open_pyra_player(GtkWidget *widget, gpointer data)   { LAUNCH_PYLIB("pyra_player.py"); }
void open_cyon_matrix(GtkWidget *widget, gpointer data)   { LAUNCH_PYLIB("cyon_matrix.py"); }
void open_cut_video(GtkWidget *widget, gpointer data)     { LAUNCH_PYLIB("cut_video.py"); }
void open_extract_audio(GtkWidget *widget, gpointer data) { LAUNCH_PYLIB("extract_audio.py"); }
void open_cut_audio(GtkWidget *widget, gpointer data)     { LAUNCH_PYLIB("cut_audio.py"); }
void open_merge_aud_vid(GtkWidget *widget, gpointer data) { LAUNCH_PYLIB("merge_aud_vid.py"); }
void open_adjust_volume(GtkWidget *widget, gpointer data) { LAUNCH_PYLIB("adjust_volume.py"); }
void open_concat_vid(GtkWidget *widget, gpointer data)    { LAUNCH_PYLIB("concat_vid.py"); }
void open_concat_aud(GtkWidget *widget, gpointer data)    { LAUNCH_PYLIB("concat_aud.py"); }

void tool_a(GtkWidget *widget, gpointer data) { g_print("Tool A selected.\n"); }
void tool_b(GtkWidget *widget, gpointer data) { g_print("Tool B selected.\n"); }
void port_scanner(GtkWidget *widget, gpointer data) { g_print("Port scanner activated\n"); }
void dns_lookup(GtkWidget *widget, gpointer data) { g_print("DNS lookup activated\n"); }
void firewall_sec(GtkWidget *widget, gpointer data) { g_print("Firewall selected.\n"); }

int is_watcher_running() {
    FILE *fp = popen("pgrep -x watcher", "r");
    if (!fp) return 0;
    char buffer[64];
    int running = (fgets(buffer, sizeof(buffer), fp) != NULL);
    pclose(fp);
    return running;
}

void watcher(GtkWidget *widget, gpointer data) {
    if (is_watcher_running()) {
        g_print("watcher is running — killing it.\n");
        system("pkill -x watcher");
        ui_log("■ WATCHER: stopped.");
        return;
    }
    const char *terminals[] = {
        "mate-terminal", "x-terminal-emulator", "gnome-terminal",
        "xfce4-terminal", "lxterminal", "konsole", "xterm"
    };
    char full_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", full_path, sizeof(full_path) - 1);
    if (len == -1) { perror("readlink"); return; }
    full_path[len] = '\0';
    char *dir = dirname(full_path);
    char watcher_path[PATH_MAX];
    snprintf(watcher_path, sizeof(watcher_path), "%s/watcher", dir);
    for (int i = 0; i < (int)(sizeof(terminals)/sizeof(terminals[0])); i++) {
        char cmd[PATH_MAX + 128];
        snprintf(cmd, sizeof(cmd), "%s -e \"%s\"", terminals[i], watcher_path);
        if (system(cmd) == 0) { ui_log("▸ WATCHER: started."); return; }
    }
    ui_log("! WATCHER: No compatible terminal found.");
}

void open_downloader(GtkWidget *widget, gpointer data) { show_downloader_window(); }

/* ── Helpers ────────────────────────────────────────────────────────────── */
typedef struct { char *msg; } LogMsg;
static gboolean log_append_idle(gpointer data) {
    LogMsg *lm = data; GtkTextIter end, start;
    gtk_text_buffer_get_end_iter(log_buffer, &end);
    int offset = gtk_text_iter_get_offset(&end);
    gtk_text_buffer_insert(log_buffer, &end, lm->msg, -1);
    gtk_text_buffer_insert(log_buffer, &end, "\n", 1);
    /* Apply color tags to lines */
    gtk_text_buffer_get_iter_at_offset(log_buffer, &start, offset);
    gtk_text_buffer_get_end_iter(log_buffer, &end);
    if (shell_tag && (strstr(lm->msg, "[SHELL]") || strstr(lm->msg, "SHELL:")))
        gtk_text_buffer_apply_tag(log_buffer, shell_tag, &start, &end);
    else if (default_tag)
        gtk_text_buffer_apply_tag(log_buffer, default_tag, &start, &end);
    gtk_text_buffer_get_end_iter(log_buffer, &end);
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(log_view), &end, 0, FALSE, 0, 0);
    g_free(lm->msg); g_free(lm); return G_SOURCE_REMOVE;
}
static void ui_log(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); char *msg = g_strdup_vprintf(fmt, ap); va_end(ap);
    LogMsg *lm = g_new(LogMsg, 1); lm->msg = msg; g_idle_add(log_append_idle, lm);
}
static GtkWidget *make_button(const char *label, const char *css_class, GCallback cb) {
    GtkWidget *btn = gtk_button_new_with_label(label);
    gtk_style_context_add_class(gtk_widget_get_style_context(btn), css_class);
    g_signal_connect(btn, "clicked", cb, NULL); 
    return btn;
}
typedef struct { int fd; GPid *pid_ptr; GtkWidget *status_lbl; char prefix[16]; } ReaderArgs;
static gpointer reader_thread(gpointer data) {
    ReaderArgs *args = data; char buf[1024]; FILE *f = fdopen(args->fd, "r");
    if (!f) { g_free(args); return NULL; }
    while (fgets(buf, sizeof buf, f)) {
        size_t len = strlen(buf); if (len > 0 && buf[len-1] == '\n') buf[len-1] = '\0';
        ui_log(" [%s] %s", args->prefix, buf);
    }
    fclose(f); ui_log("▸ %s disconnected.", args->prefix); *args->pid_ptr = 0; g_free(args); return NULL;
}
/* ── Callbacks ──────────────────────────────────────────────────────────── */
static void on_input_activate(GtkEntry *entry, gpointer data) {
    const char *text = gtk_entry_get_text(entry);
    if (!text || strlen(text) == 0) return;

    char buffer[2048];
    snprintf(buffer, sizeof(buffer), "%s\n", text);

    /* Always send to shell for echo and commands */
    if (shell_pid != 0)
        write(shell_stdin_fd, buffer, strlen(buffer));
    else
        ui_log(" You: %s", text);  /* fallback echo if shell not running */

    gtk_entry_set_text(entry, "");

    /* Forward non-commands to bot if running */
    if (text[0] != '/' && local_pid != 0)
        write(local_stdin_fd, buffer, strlen(buffer));
}
static void update_status_label(GtkWidget *label, gboolean online) {
    GtkStyleContext *ctx = gtk_widget_get_style_context(label);
    if (online) {
        gtk_label_set_text(GTK_LABEL(label), "● ONLINE");
        gtk_style_context_remove_class(ctx, "status-off");
        gtk_style_context_add_class(ctx, "status-on");
    } else {
        gtk_label_set_text(GTK_LABEL(label), "● OFFLINE");
        gtk_style_context_remove_class(ctx, "status-on");
        gtk_style_context_add_class(ctx, "status-off");
    }
}
static void stop_ollama(GtkWidget *b, gpointer d) { 
    if(ollama_pid > 0) { kill(ollama_pid, SIGTERM); ollama_pid = 0; update_status_label(ollama_status_lbl, FALSE); ui_log("■ OLLAMA: Server terminated."); } 
}
static void start_ollama(GtkWidget *b, gpointer d) {
    if (ollama_pid > 0) return;
    gchar *argv[] = { "ollama", "serve", NULL };
    if (g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &ollama_pid, NULL, &ollama_fd, NULL, NULL)) {
        update_status_label(ollama_status_lbl, TRUE); ui_log("▸ OLLAMA: Server live.");
    }
}
static void stop_bot(GtkWidget *b, gpointer d) { 
    if(bot_pid > 0) { kill(bot_pid, SIGTERM); bot_pid = 0; update_status_label(bot_status_lbl, FALSE); ui_log("■ DISCORD: Connection severed."); } 
}
static void start_bot(GtkWidget *b, gpointer d) {
    if (bot_pid > 0) return;
    gchar *argv[] = { VENV_PYTHON, BOT_SCRIPT, NULL };
    if (g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &bot_pid, NULL, &bot_fd, NULL, NULL)) {
        update_status_label(bot_status_lbl, TRUE); ui_log("▸ DISCORD: Bot protocol initialized.");
    }
}
static void start_shell(void) {
    if (shell_pid > 0) return;
    gchar *argv[] = { VENV_PYTHON, SHELL_SCRIPT, NULL };
    if (g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &shell_pid, &shell_stdin_fd, &shell_stdout_fd, NULL, NULL)) {
        ReaderArgs *args = g_new(ReaderArgs, 1);
        args->fd = shell_stdout_fd; args->pid_ptr = &shell_pid; g_strlcpy(args->prefix, "SHELL", 16);
        g_thread_new("shell-reader", reader_thread, args);
        ui_log("\u25b8 SHELL: Command processor online.");
    }
}
static void stop_local(GtkWidget *b, gpointer d) { 
    if(local_pid > 0) { kill(local_pid, SIGTERM); local_pid = 0; update_status_label(local_status_lbl, FALSE); ui_log("■ LOCAL: Logic bridge closed."); } 
}
static void start_local(GtkWidget *b, gpointer d) {
    if (local_pid > 0) return;
    gchar *argv[] = { VENV_PYTHON, LOCAL_BOT_SCRIPT, NULL };
    if (g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &local_pid, &local_stdin_fd, &local_stdout_fd, NULL, NULL)) {
        update_status_label(local_status_lbl, TRUE);
        ReaderArgs *args = g_new(ReaderArgs, 1);
        args->fd = local_stdout_fd; args->pid_ptr = &local_pid; g_strlcpy(args->prefix, "LOCAL", 16);
        g_thread_new("local-reader", reader_thread, args);
        ui_log("▸ LOCAL: Cyon core linked.");
    }
}
static void stop_all(GtkWidget *b, gpointer d) { 
    ui_log("!!! EMERGENCY SHUTDOWN INITIATED !!!");
    stop_ollama(NULL,NULL); stop_bot(NULL,NULL); stop_local(NULL,NULL); 
}

/* ── Clean shutdown: kill ALL child processes then quit ─────────────────── */
static void shutdown_all(void) {
    if (ollama_pid > 0) { kill(ollama_pid, SIGTERM); ollama_pid = 0; }
    if (bot_pid    > 0) { kill(bot_pid,    SIGTERM); bot_pid    = 0; }
    if (local_pid  > 0) { kill(local_pid,  SIGTERM); local_pid  = 0; }
    if (shell_pid  > 0) { kill(shell_pid,  SIGTERM); shell_pid  = 0; }
    gtk_main_quit();
}

static gboolean on_delete_event(GtkWidget *w, GdkEvent *e, gpointer d) {
    shutdown_all();
    return TRUE; /* we handled it — prevent double-destroy */
}
/* ── Main ───────────────────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    resolve_paths();
    const char *cyon_home = getenv("CYON_HOME");
    const char *home      = getenv("HOME");
    const char *base      = cyon_home ? cyon_home : (home ? ({ static char b[PATH_MAX]; snprintf(b, sizeof(b), "%s/cyon", home); b; }) : "/cyon");
    char intro_cmd[PATH_MAX + 64];
    snprintf(intro_cmd, sizeof(intro_cmd), "ffplay -nodisp -autoexit %s/assets/intro_sound.mp3 &", base);
    system(intro_cmd);
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, CSS, -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(provider), 800);

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(win), 580, 650);
    g_signal_connect(win, "delete-event", G_CALLBACK(on_delete_event), NULL);

    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(outer), 15);

    /* ── Menu Bar ─────────────────────────────────────────────────────── */
    GtkWidget *menubar = gtk_menu_bar_new();

    /* Programs menu */
    GtkWidget *programs_menu = gtk_menu_new();
    GtkWidget *programs_item = gtk_menu_item_new_with_label("Programs");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(programs_item), programs_menu);

    GtkWidget *pyra_player_item = gtk_menu_item_new_with_label("Pyra Player");
    gtk_menu_shell_append(GTK_MENU_SHELL(programs_menu), pyra_player_item);
    g_signal_connect(pyra_player_item, "activate", G_CALLBACK(open_pyra_player), NULL);

    GtkWidget *downloader_item = gtk_menu_item_new_with_label("Downloader");
    gtk_menu_shell_append(GTK_MENU_SHELL(programs_menu), downloader_item);
    g_signal_connect(downloader_item, "activate", G_CALLBACK(open_downloader), NULL);

    GtkWidget *cyon_matrix_item = gtk_menu_item_new_with_label("Cyon Matrix");
    gtk_menu_shell_append(GTK_MENU_SHELL(programs_menu), cyon_matrix_item);
    g_signal_connect(cyon_matrix_item, "activate", G_CALLBACK(open_cyon_matrix), NULL);

    GtkWidget *cli_item = gtk_menu_item_new_with_label("Cyon CLI");
    gtk_menu_shell_append(GTK_MENU_SHELL(programs_menu), cli_item);
    g_signal_connect(cli_item, "activate", G_CALLBACK(launch_cli), NULL);

    GtkWidget *pyra_item = gtk_menu_item_new_with_label("Pyra CLI");
    gtk_menu_shell_append(GTK_MENU_SHELL(programs_menu), pyra_item);
    g_signal_connect(pyra_item, "activate", G_CALLBACK(launch_pyra), NULL);

    /* Tools submenu */
    GtkWidget *tools_item = gtk_menu_item_new_with_label("Tools");
    GtkWidget *tools_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(tools_item), tools_menu);

    /* Editor Tools submenu */
    GtkWidget *editor_tools_item = gtk_menu_item_new_with_label("editor tools");
    GtkWidget *editor_tools_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(editor_tools_item), editor_tools_menu);

    GtkWidget *gtk_cut_video_item = gtk_menu_item_new_with_label("cut video");
    gtk_menu_shell_append(GTK_MENU_SHELL(editor_tools_menu), gtk_cut_video_item);
    g_signal_connect(gtk_cut_video_item, "activate", G_CALLBACK(open_cut_video), NULL);

    GtkWidget *gtk_extract_audio_item = gtk_menu_item_new_with_label("extract audio");
    gtk_menu_shell_append(GTK_MENU_SHELL(editor_tools_menu), gtk_extract_audio_item);
    g_signal_connect(gtk_extract_audio_item, "activate", G_CALLBACK(open_extract_audio), NULL);

    GtkWidget *gtk_cut_audio_item = gtk_menu_item_new_with_label("cut audio");
    gtk_menu_shell_append(GTK_MENU_SHELL(editor_tools_menu), gtk_cut_audio_item);
    g_signal_connect(gtk_cut_audio_item, "activate", G_CALLBACK(open_cut_audio), NULL);

    GtkWidget *gtk_merge_aud_vid_item = gtk_menu_item_new_with_label("merge audio/video");
    gtk_menu_shell_append(GTK_MENU_SHELL(editor_tools_menu), gtk_merge_aud_vid_item);
    g_signal_connect(gtk_merge_aud_vid_item, "activate", G_CALLBACK(open_merge_aud_vid), NULL);

    GtkWidget *gtk_adjust_volume_item = gtk_menu_item_new_with_label("adjust volume");
    gtk_menu_shell_append(GTK_MENU_SHELL(editor_tools_menu), gtk_adjust_volume_item);
    g_signal_connect(gtk_adjust_volume_item, "activate", G_CALLBACK(open_adjust_volume), NULL);

    GtkWidget *gtk_concat_vid_item = gtk_menu_item_new_with_label("stitch videos");
    gtk_menu_shell_append(GTK_MENU_SHELL(editor_tools_menu), gtk_concat_vid_item);
    g_signal_connect(gtk_concat_vid_item, "activate", G_CALLBACK(open_concat_vid), NULL);

    GtkWidget *gtk_concat_aud_item = gtk_menu_item_new_with_label("stitch audio");
    gtk_menu_shell_append(GTK_MENU_SHELL(editor_tools_menu), gtk_concat_aud_item);
    g_signal_connect(gtk_concat_aud_item, "activate", G_CALLBACK(open_concat_aud), NULL);

    GtkWidget *gtk_convert_item = gtk_menu_item_new_with_label("convert pics");
    GtkWidget *gtk_tarmaker_item = gtk_menu_item_new_with_label("create tarfile");

    /* NEW: Piper TTS right after tarfile */
    GtkWidget *gtk_piper_tts_item = gtk_menu_item_new_with_label("Pyra Notes/TTS");

    GtkWidget *tool_subitem1 = gtk_menu_item_new_with_label("Tool A");
    GtkWidget *tool_subitem2 = gtk_menu_item_new_with_label("Tool B");

    /* Append items to Tools submenu in order */
    gtk_menu_shell_append(GTK_MENU_SHELL(tools_menu), editor_tools_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(tools_menu), gtk_convert_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(tools_menu), gtk_tarmaker_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(tools_menu), gtk_piper_tts_item);  // <--- added here
    gtk_menu_shell_append(GTK_MENU_SHELL(tools_menu), tool_subitem1);
    gtk_menu_shell_append(GTK_MENU_SHELL(tools_menu), tool_subitem2);
    gtk_menu_shell_append(GTK_MENU_SHELL(programs_menu), tools_item);

    /* Connect callbacks */
    g_signal_connect(gtk_convert_item, "activate", G_CALLBACK(open_gtk_convert), NULL);
    g_signal_connect(gtk_tarmaker_item, "activate", G_CALLBACK(create_tarfile), NULL);
    g_signal_connect(gtk_piper_tts_item, "activate", G_CALLBACK(open_pyra_notes), NULL);
    g_signal_connect(tool_subitem1, "activate", G_CALLBACK(tool_a), NULL);
    g_signal_connect(tool_subitem2, "activate", G_CALLBACK(tool_b), NULL);

    /* Security submenu */
    GtkWidget *security_item = gtk_menu_item_new_with_label("Security");
    GtkWidget *security_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(security_item), security_menu);

    GtkWidget *defense_item = gtk_menu_item_new_with_label("Defense");
    GtkWidget *defense_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(defense_item), defense_menu);

    GtkWidget *firewall_item = gtk_menu_item_new_with_label("Firewall");
    GtkWidget *watcher_item = gtk_menu_item_new_with_label("Watcher");
    gtk_menu_shell_append(GTK_MENU_SHELL(defense_menu), firewall_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(defense_menu), watcher_item);
    g_signal_connect(firewall_item, "activate", G_CALLBACK(firewall_sec), NULL);
    g_signal_connect(watcher_item, "activate", G_CALLBACK(watcher), NULL);

    GtkWidget *offense_item = gtk_menu_item_new_with_label("Offense");
    GtkWidget *offense_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(offense_item), offense_menu);

    GtkWidget *scanner_item = gtk_menu_item_new_with_label("Port Scanner");
    GtkWidget *dns_item = gtk_menu_item_new_with_label("DNS Lookup");
    GtkWidget *repeater_item = gtk_menu_item_new_with_label("Pyra Repeater");
    gtk_menu_shell_append(GTK_MENU_SHELL(offense_menu), scanner_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(offense_menu), dns_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(offense_menu), repeater_item);
    g_signal_connect(scanner_item, "activate", G_CALLBACK(port_scanner), NULL);
    g_signal_connect(dns_item, "activate", G_CALLBACK(dns_lookup), NULL);
    g_signal_connect(repeater_item, "activate", G_CALLBACK(launch_repeater), NULL);

    gtk_menu_shell_append(GTK_MENU_SHELL(security_menu), defense_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(security_menu), offense_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(programs_menu), security_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), programs_item);

    /* Pack menubar FIRST, then border-padded content */
    gtk_box_pack_start(GTK_BOX(outer), menubar, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(win), outer);

    /* ── Title ────────────────────────────────────────────────────────── */
    GtkWidget *title = gtk_label_new("▸ CYON CONTROL PANEL");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "title-label");
    gtk_box_pack_start(GTK_BOX(outer), title, FALSE, FALSE, 0);

    /* OLLAMA */
    GtkWidget *ol_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    ollama_status_lbl = gtk_label_new("● OFFLINE");
    gtk_style_context_add_class(gtk_widget_get_style_context(ollama_status_lbl), "status-label");
    gtk_style_context_add_class(gtk_widget_get_style_context(ollama_status_lbl), "status-off");
    gtk_box_pack_start(GTK_BOX(ol_box), gtk_label_new("OLLAMA SERVER"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ol_box), ollama_status_lbl, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(ol_box), make_button("STOP", "btn-stop", G_CALLBACK(stop_ollama)), FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(ol_box), make_button("START", "btn-start", G_CALLBACK(start_ollama)), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(outer), ol_box, FALSE, FALSE, 0);

    /* DISCORD */
    GtkWidget *ds_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    bot_status_lbl = gtk_label_new("● OFFLINE");
    gtk_style_context_add_class(gtk_widget_get_style_context(bot_status_lbl), "status-label");
    gtk_style_context_add_class(gtk_widget_get_style_context(bot_status_lbl), "status-off");
    gtk_box_pack_start(GTK_BOX(ds_box), gtk_label_new("DISCORD BOT"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ds_box), bot_status_lbl, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(ds_box), make_button("STOP", "btn-stop", G_CALLBACK(stop_bot)), FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(ds_box), make_button("START", "btn-start", G_CALLBACK(start_bot)), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(outer), ds_box, FALSE, FALSE, 0);

    /* LOCAL */
    GtkWidget *lc_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    local_status_lbl = gtk_label_new("● OFFLINE");
    gtk_style_context_add_class(gtk_widget_get_style_context(local_status_lbl), "status-label");
    gtk_style_context_add_class(gtk_widget_get_style_context(local_status_lbl), "status-off");
    gtk_box_pack_start(GTK_BOX(lc_box), gtk_label_new("LOCAL CYON"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(lc_box), local_status_lbl, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(lc_box), make_button("STOP", "btn-stop", G_CALLBACK(stop_local)), FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(lc_box), make_button("START", "btn-start", G_CALLBACK(start_local)), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(outer), lc_box, FALSE, FALSE, 0);

    GtkWidget *stop_all_btn = make_button("■ SHUTDOWN ALL", "btn-stop", G_CALLBACK(stop_all));
    gtk_box_pack_start(GTK_BOX(outer), stop_all_btn, FALSE, FALSE, 5);

    /* ── Marquee strip ───────────────────────────────────────────────── */
    GtkWidget *msep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(outer), msep, FALSE, FALSE, 2);
    marquee_label = gtk_label_new("");
    gtk_style_context_add_class(gtk_widget_get_style_context(marquee_label), "main-marquee");
    gtk_widget_set_halign(marquee_label, GTK_ALIGN_FILL);
    gtk_label_set_selectable(GTK_LABEL(marquee_label), FALSE);
    gtk_box_pack_start(GTK_BOX(outer), marquee_label, FALSE, FALSE, 2);
    GtkWidget *msep2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(outer), msep2, FALSE, FALSE, 2);
    /* kick off marquee */
    snprintf(marquee_buf, sizeof(marquee_buf),
             "%-*s%s", MAIN_MARQUEE_WIDTH, "", MAIN_MARQUEE_MSGS[0]);
    g_timeout_add(80, main_tick_marquee, NULL);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scroll), 250);
    log_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(log_view), FALSE);
    gtk_style_context_add_class(gtk_widget_get_style_context(log_view), "log-view");
    log_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(log_view));
    shell_tag = gtk_text_buffer_create_tag(log_buffer, "shell_cyan", "foreground", "#00e5ff", NULL);
    default_tag = gtk_text_buffer_create_tag(log_buffer, "default_green", "foreground", "#00cc77", NULL);
    gtk_container_add(GTK_CONTAINER(scroll), log_view);
    gtk_box_pack_start(GTK_BOX(outer), scroll, TRUE, TRUE, 0);

    GtkWidget *input_field = gtk_entry_new();
    g_signal_connect(input_field, "activate", G_CALLBACK(on_input_activate), NULL);
    gtk_box_pack_start(GTK_BOX(outer), input_field, FALSE, FALSE, 0);

    gtk_widget_show_all(win);
    start_shell();  /* auto-start command shell */
    gtk_main();
    return 0;
}
