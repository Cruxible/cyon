/* cyon_notes.c — CYON notes editor + Piper TTS
 * Author: Ioannes Cruxibulum
 *
 * Build:
 *   gcc $(pkg-config --cflags gtk+-3.0) -o cyon_notes cyon_notes.c \
 *       $(pkg-config --libs gtk+-3.0) -lpthread
 *
 * Stage 1+: core editor + file tree — tabs, line numbers, file ops, FILE menu,
 *          Ctrl+S/Z/Y/T, undo/redo, auto-indent, auto-close brackets,
 *          Ctrl+scroll zoom, single-instance lock.
 * Stage 2: syntax highlighting, cyon_notes.conf (GKeyFile + GRegex)
 * Stage 3 (next): TTS (piper + aplay), voice switch
 */

#include <gtk/gtk.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>

static GtkTextTagTable *g_shared_tags = NULL;

/* ── constants ──────────────────────────────────────────────────────────── */

#define APP_TITLE       "▸ CYON NOTES // TTS"
#define NOTES_DIR_NAME  "Documents/pyra_dev_notes"
#define LOCK_FILE       "/tmp/cyon_notes.lock"
#define MAX_UNDO        300
#define CONFIG_FILENAME "cyon_notes.conf"
#define DEFAULT_FONT_PX 15
#define MIN_FONT_PX     8
#define MAX_FONT_PX     36

/* ── CSS ────────────────────────────────────────────────────────────────── */

static const char *APP_CSS =
"window {"
"  background-color: #05050a;"
"}"
".title-label {"
"  font-family: monospace;"
"  font-size: 13px;"
"  font-weight: bold;"
"  color: #00ff99;"
"  letter-spacing: 2px;"
"}"
"#editor-view, #editor-view text {"
"  background-color: #05050a;"
"  color: #00ff99;"
"  font-family: monospace;"
"  font-size: 15px;"
"  caret-color: #00ff99;"
"}"
"#line-numbers, #line-numbers text {"
"  background-color: #080810;"
"  color: #2a4a38;"
"  font-family: monospace;"
"  font-size: 15px;"
"}"
"scrolledwindow { border: 1px solid #1a2a20; }"
"entry { background-color: #080810; color: #00ff99; font-family: monospace; font-size: 11px; border: 1px solid #1a4a28; caret-color: #00ff99; }"
"entry:focus { border-color: #E8A020; }"
"notebook > header {"
"  background-color: #080810;"
"  border-bottom: 1px solid #1a2a20;"
"  padding: 0;"
"}"
"notebook > header > tabs > tab {"
"  background-color: #0d0d15;"
"  color: #336655;"
"  font-family: monospace;"
"  font-size: 11px;"
"  padding: 4px 8px;"
"  border: 1px solid #1a2a20;"
"  border-bottom: none;"
"  margin-right: 2px;"
"}"
"notebook > header > tabs > tab:checked {"
"  background-color: #05050a;"
"  color: #00ff99;"
"  border-color: #E8A020;"
"}"
".btn-menu {"
"  background-color: #0a1a10;"
"  color: #00ff99;"
"  font-family: monospace;"
"  font-size: 11px;"
"  border: 1px solid #1a4a28;"
"  border-radius: 2px;"
"  padding: 3px 10px;"
"}"
".btn-menu:hover { background-color: #0f2a18; border-color: #E8A020; }"
".btn-danger {"
"  background-color: #1a0a0a;"
"  color: #ff4444;"
"  font-family: monospace;"
"  font-size: 11px;"
"  border: 1px solid #4a1a1a;"
"  border-radius: 2px;"
"  padding: 3px 10px;"
"}"
".section-label {"
"  font-family: monospace;"
"  font-size: 11px;"
"  color: #336655;"
"}"
".status-bar {"
"  font-family: monospace;"
"  font-size: 10px;"
"  color: #1a4a30;"
"}"
"#log-view, #log-view text {"
"  background-color: #030308;"
"  color: #00cc66;"
"  font-family: monospace;"
"  font-size: 10px;"
"}"
".tab-close-btn {"
"  padding: 0 2px;"
"  min-width: 0;"
"  min-height: 0;"
"  border: none;"
"  background: transparent;"
"  color: #336655;"
"  font-family: monospace;"
"  font-size: 10px;"
"}"
".tab-close-btn:hover { color: #ff4444; border-color: #E8A020; }"

    ".tree-panel {"
    "  background-color: #080810;"
    "  border-left: 1px solid #1a2a20;"
    "}"

    ".tree-header {"
    "  font-family: monospace;"
    "  font-size: 11px;"
    "  color: #00ff99;"
    "  font-weight: bold;"
    "}"

    ".btn-tree {"
    "  background-color: #0a1a12;"
    "  color: #00dd88;"
    "  font-family: monospace;"
    "  font-size: 13px;"
    "  border: 1px solid #2a4a38;"
    "  border-radius: 3px;"
    "  padding: 4px 10px;"
    "  min-width: 0;"
    "}"
    ".btn-tree:hover { color: #00ff99; border-color: #E8A020; }"

    "#tree-view {"
    "  background-color: #080810;"
    "  color: #00cc77;"
    "  font-family: monospace;"
    "  font-size: 11px;"
    "}"

    "#tree-view:selected {"
    "  background-color: #0a2a18;"
    "  color: #00ff99;"
    "}";

/* ── undo entry ─────────────────────────────────────────────────────────── */

typedef struct {
    char **entries;   /* array of g_strdup'd snapshots */
    int    count;
    int    capacity;
} UndoStack;

static void undo_stack_init(UndoStack *s) {
    s->capacity = 64;
    s->entries  = g_malloc(s->capacity * sizeof(char*));
    s->count    = 0;
}

static void undo_stack_free(UndoStack *s) {
    for (int i = 0; i < s->count; i++)
        g_free(s->entries[i]);
    g_free(s->entries);
    s->count = 0;
}

static void undo_stack_push(UndoStack *s, const char *text) {
    /* avoid duplicate top */
    if (s->count > 0 && strcmp(s->entries[s->count-1], text) == 0)
        return;
    /* trim oldest if at max */
    if (s->count >= MAX_UNDO) {
        g_free(s->entries[0]);
        memmove(s->entries, s->entries+1, (s->count-1)*sizeof(char*));
        s->count--;
    }
    if (s->count >= s->capacity) {
        s->capacity *= 2;
        s->entries = g_realloc(s->entries, s->capacity * sizeof(char*));
    }
    s->entries[s->count++] = g_strdup(text);
}

static const char *undo_stack_top(UndoStack *s) {
    return s->count > 0 ? s->entries[s->count-1] : NULL;
}

static void undo_stack_clear(UndoStack *s) {
    for (int i = 0; i < s->count; i++)
        g_free(s->entries[i]);
    s->count = 0;
}

/* ── EditorTab ──────────────────────────────────────────────────────────── */

typedef struct _AppState AppState;  /* forward */

typedef struct {
    /* widgets */
    GtkWidget     *scroll;        /* the notebook page widget */
    GtkWidget     *text_view;
    GtkWidget     *line_view;
    GtkTextBuffer *text_buf;
    GtkTextBuffer *line_buf;
    GtkWidget     *tab_label;     /* GtkLabel in the tab header */

    /* state */
    char          *current_file;  /* g_strdup'd path or NULL */
    UndoStack      undo_stack;
    UndoStack      redo_stack;
    gboolean       undo_inhibit;
    gboolean       is_dirty;      /* unsaved changes */

    AppState      *app;           /* back-pointer */
} EditorTab;

/* ── AppState ───────────────────────────────────────────────────────────── */

struct _AppState {
    GtkWidget     *window;
    GtkWidget     *notebook;
    GtkWidget     *filename_entry;
    GtkWidget     *status_label;
    GtkWidget     *log_view;
    GtkTextBuffer *log_buf;
    GtkWidget     *file_menu;
    GtkWidget     *tree_toggle_item;  /* for label update — stage 2 */

    int            font_size;         /* current px */
    GtkCssProvider *size_provider;

    /* syntax highlighting */
    GtkTextTag   **hl_kw_tags;   /* one tag per highlight_group_N */
    GRegex       **hl_kw_regex;  /* compiled regex per group */
    int            hl_group_count;
    GtkTextTag    *hl_tag_lime;
    GtkTextTag    *hl_tag_coral;
    GtkTextTag    *hl_tag_comment;
    GtkTextTag    *hl_tag_dot_l;
    GtkTextTag    *hl_tag_dot_r;
    GtkTextTag    *hl_tag_steel; /* = operator, @staticmethod */

    /* TTS */
    GPid           tts_pid;         /* currently running piper pid, 0=none */
    gboolean       tts_voice_joe;   /* TRUE=joe (male), FALSE=lessac (female) */
    GtkWidget     *tts_menu;        /* popup */

    /* cursor pos label */
    GtkWidget     *cursor_label;

    /* file tree */
    GtkWidget     *tree_panel;
    GtkWidget     *tree_view;
    GtkTreeStore  *tree_store;
    GtkWidget     *tree_toggle_item_widget;
    char          *tree_folder;
    GFileMonitor  *tree_monitor;
    guint          tree_refresh_timer;
    gboolean       tree_save_in_progress;  /* suppress monitor during save */
};


/* ── forward declarations ───────────────────────────────────────────────── */
/* ── forward declarations ───────────────────────────────────────────────── */

static EditorTab *app_current_tab(AppState *app);
static EditorTab *tab_new(AppState *app, const char *path);
static void       tab_load_file(EditorTab *tab, const char *path);
static void       tab_update_line_numbers(EditorTab *tab);
static void       tab_mark_clean(EditorTab *tab);
static gboolean   tab_confirm_close(EditorTab *tab);
static char       *tab_display_name(EditorTab *tab);

static void app_log(AppState *app, const char *fmt, ...);
static void tree_populate(AppState *app, const char *folder);
static void on_tree_toggle(GtkMenuItem *item, AppState *app);
static void app_apply_font_size(AppState *app);
static void hl_load_config(AppState *app);
static void hl_apply(AppState *app, GtkTextBuffer *buf);
static void on_tts_stop(GtkMenuItem *item, AppState *app);
static void update_cursor_label(AppState *app);

/* ── ADD THESE ─────────────────────────────────────────────────────────── */
static void on_cursor_moved(GtkTextBuffer *buffer, GtkTextIter *location,
                             GtkTextMark *mark, AppState *app);

static void toggle_comment(AppState *app);
static void duplicate_line(AppState *app);

static void on_new(GtkMenuItem *item, AppState *app);
static void on_new_tab(GtkMenuItem *item, AppState *app);
static void on_load(GtkMenuItem *item, AppState *app);
static void on_save(GtkMenuItem *item, AppState *app);
static void on_save_as(GtkMenuItem *item, AppState *app);
static void on_delete_file(GtkMenuItem *item, AppState *app);
static void on_undo(GtkMenuItem *item, AppState *app);
static void on_redo(GtkMenuItem *item, AppState *app);
static void on_text_size_inc(GtkMenuItem *item, AppState *app);
static void on_text_size_dec(GtkMenuItem *item, AppState *app);

/* ── ADD THIS ONE ───────────────────────────────────────────────────────── */
/* ── single instance lock ───────────────────────────────────────────────── */

static gboolean acquire_lock(void) {
    FILE *f = fopen(LOCK_FILE, "r");
    if (f) {
        int pid = 0;
        fscanf(f, "%d", &pid);
        fclose(f);
        if (pid > 0 && kill(pid, 0) == 0) {
            /* process is alive */
            return FALSE;
        }
    }
    f = fopen(LOCK_FILE, "w");
    if (f) {
        fprintf(f, "%d\n", (int)getpid());
        fclose(f);
    }
    return TRUE;
}

static void release_lock(void) {
    unlink(LOCK_FILE);
}

/* ── notes dir ──────────────────────────────────────────────────────────── */

static char *get_notes_dir(void) {
    const char *home = g_get_home_dir();
    return g_build_filename(home, NOTES_DIR_NAME, NULL);
}

static void ensure_notes_dir(void) {
    char *dir = get_notes_dir();
    g_mkdir_with_parents(dir, 0755);
    g_free(dir);
}

/* ── tab_display_name ───────────────────────────────────────────────────── */

/* returns g_strdup string — caller must g_free() */
static char *tab_display_name(EditorTab *tab) {
    if (!tab->current_file)
        return g_strdup("untitled");
    char *base = g_path_get_basename(tab->current_file);
    char *safe = g_utf8_make_valid(base, -1);
    g_free(base);
    size_t len = strlen(safe);
    char *result = (len > 4 && strcmp(safe + len - 4, ".txt") == 0)
        ? g_strndup(safe, len - 4) : g_strdup(safe);
    g_free(safe);
    return result;
}

/* ── update tab label ───────────────────────────────────────────────────── */

static void tab_update_label(EditorTab *tab) {
    char *base = tab_display_name(tab);
    char *label = tab->is_dirty
        ? g_strdup_printf("● %s", base) : g_strdup(base);
    g_free(base);
    gtk_label_set_text(GTK_LABEL(tab->tab_label), label);
    g_free(label);
}

/* ── line numbers ───────────────────────────────────────────────────────── */

static void tab_update_line_numbers(EditorTab *tab) {
    int count = gtk_text_buffer_get_line_count(tab->text_buf);
    /* width = digits in count */
    int width = 1, n = count;
    while (n >= 10) { width++; n /= 10; }

    GString *nums = g_string_new(NULL);
    for (int i = 1; i <= count; i++) {
        g_string_append_printf(nums, "%*d", width, i);
        if (i < count)
            g_string_append_c(nums, '\n');
    }
    gtk_text_buffer_set_text(tab->line_buf, nums->str, -1);
    g_string_free(nums, TRUE);
}

/* ── sync line scroll ───────────────────────────────────────────────────── */

static void on_line_scroll_sync(GtkAdjustment *adj, EditorTab *tab) {
    gdouble val = gtk_adjustment_get_value(adj);
    GtkAdjustment *ladj = gtk_scrollable_get_vadjustment(
        GTK_SCROLLABLE(tab->line_view));
    if (ladj)
        gtk_adjustment_set_value(ladj, val);
}

/* ── undo/redo ──────────────────────────────────────────────────────────── */

static void tab_mark_clean(EditorTab *tab) {
    GtkTextIter s, e;
    gtk_text_buffer_get_bounds(tab->text_buf, &s, &e);
    char *text = gtk_text_buffer_get_text(tab->text_buf, &s, &e, FALSE);
    /* reset base snapshot */
    undo_stack_clear(&tab->undo_stack);
    undo_stack_push(&tab->undo_stack, text);
    g_free(text);
    tab->is_dirty = FALSE;
}

static void on_text_changed(GtkTextBuffer *buf, EditorTab *tab) {
    if (!tab->undo_inhibit) {
        GtkTextIter s, e;
        gtk_text_buffer_get_bounds(buf, &s, &e);
        char *snap = gtk_text_buffer_get_text(buf, &s, &e, FALSE);
        undo_stack_push(&tab->undo_stack, snap);
        undo_stack_clear(&tab->redo_stack);
        g_free(snap);
        tab->is_dirty = TRUE;
    }
    hl_apply(tab->app, buf);
    tab_update_line_numbers(tab);
}

/* ── tab_load_file ──────────────────────────────────────────────────────── */

static void tab_load_file(EditorTab *tab, const char *path) {
    gchar *content = NULL;
    gsize  length  = 0;
    GError *err    = NULL;

    if (!g_file_get_contents(path, &content, &length, &err)) {
        app_log(tab->app, "▸ ERROR loading: %s", err->message);
        g_error_free(err);
        return;
    }

    tab->undo_inhibit = TRUE;
    gtk_text_buffer_set_text(tab->text_buf, content, -1);
    tab->undo_inhibit = FALSE;
    g_free(content);

    undo_stack_clear(&tab->undo_stack);
    undo_stack_clear(&tab->redo_stack);

    /* push initial snapshot */
    GtkTextIter s, e;
    gtk_text_buffer_get_bounds(tab->text_buf, &s, &e);
    char *snap = gtk_text_buffer_get_text(tab->text_buf, &s, &e, FALSE);
    undo_stack_push(&tab->undo_stack, snap);
    g_free(snap);

    g_free(tab->current_file);
    tab->current_file = g_strdup(path);
    tab->is_dirty     = FALSE;

    hl_apply(tab->app, tab->text_buf);
    tab_update_line_numbers(tab);
    tab_update_label(tab);
    app_log(tab->app, "▸ Loaded: %s", path);
}

/* ── confirm close (unsaved changes dialog) ─────────────────────────────── */

static gboolean tab_confirm_close(EditorTab *tab) {
    if (!tab->is_dirty)
        return TRUE;

    GtkWidget *dlg = gtk_message_dialog_new(
        GTK_WINDOW(tab->app->window),
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_WARNING,
        GTK_BUTTONS_NONE,
        "▸ '%s' has unsaved changes.", tab_display_name(tab));
    gtk_message_dialog_format_secondary_text(
        GTK_MESSAGE_DIALOG(dlg), "Save before closing?");
    gtk_dialog_add_button(GTK_DIALOG(dlg), "Discard", GTK_RESPONSE_REJECT);
    gtk_dialog_add_button(GTK_DIALOG(dlg), "Cancel",  GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dlg), "Save",    GTK_RESPONSE_ACCEPT);
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_ACCEPT);

    gint resp = gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);

    if (resp == GTK_RESPONSE_ACCEPT) {
        on_save(NULL, tab->app);
        return TRUE;
    }
    if (resp == GTK_RESPONSE_REJECT)
        return TRUE;
    return FALSE;  /* Cancel */
}

/* ── tab_new ────────────────────────────────────────────────────────────── */

static EditorTab *app_current_tab(AppState *app) {
    int idx = gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook));
    if (idx < 0) return NULL;
    GtkWidget *page = gtk_notebook_get_nth_page(
        GTK_NOTEBOOK(app->notebook), idx);
    return g_object_get_data(G_OBJECT(page), "editor-tab");
}

/* forward — defined after key handler */
static gboolean on_key_press(GtkWidget *w, GdkEventKey *ev, AppState *app);
static gboolean on_scroll_zoom(GtkWidget *w, GdkEventScroll *ev, AppState *app);

static void on_tab_close_clicked(GtkButton *btn, GtkWidget *page);

static EditorTab *tab_new(AppState *app, const char *path) {
    EditorTab *tab = g_new0(EditorTab, 1);
    tab->app          = app;
    tab->current_file = NULL;
    tab->undo_inhibit = FALSE;
    tab->is_dirty     = FALSE;
    undo_stack_init(&tab->undo_stack);
    undo_stack_init(&tab->redo_stack);

    /* ── line number view ─────────────────────────────────────────── */
    tab->line_buf  = gtk_text_buffer_new(NULL);
    /* ensure shared tag table exists before creating text buffer */
    if (!g_shared_tags) g_shared_tags = gtk_text_tag_table_new();
    tab->line_view = gtk_text_view_new_with_buffer(tab->line_buf);
    gtk_widget_set_name(tab->line_view, "line-numbers");
    gtk_text_view_set_editable(GTK_TEXT_VIEW(tab->line_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(tab->line_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(tab->line_view), GTK_WRAP_NONE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(tab->line_view), 6);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(tab->line_view), 6);
    gtk_widget_set_can_focus(tab->line_view, FALSE);

    /* ── editor view ──────────────────────────────────────────────── */
    tab->text_buf  = gtk_text_buffer_new(g_shared_tags);
    tab->text_view = gtk_text_view_new_with_buffer(tab->text_buf);
    gtk_widget_set_name(tab->text_view, "editor-view");
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(tab->text_view), GTK_WRAP_NONE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(tab->text_view), 6);

    /* ── editor_box inside scroll ─────────────────────────────────── */
    GtkWidget *editor_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(editor_box), tab->line_view, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(editor_box), tab->text_view, TRUE,  TRUE,  0);

    tab->scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tab->scroll),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(tab->scroll, -1, 340);
    gtk_container_add(GTK_CONTAINER(tab->scroll), editor_box);

    /* sync line numbers scroll */
    GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(
        GTK_SCROLLED_WINDOW(tab->scroll));
    g_signal_connect(vadj, "value-changed",
        G_CALLBACK(on_line_scroll_sync), tab);

    /* text changed */
    g_signal_connect(tab->text_buf, "changed",
        G_CALLBACK(on_text_changed), tab);

    /* key + scroll */
    g_signal_connect(tab->text_view, "key-press-event",
        G_CALLBACK(on_key_press), app);
    g_signal_connect(tab->text_view, "scroll-event",
        G_CALLBACK(on_scroll_zoom), app);
    g_signal_connect(tab->text_buf, "mark-set",
        G_CALLBACK(on_cursor_moved), app);

    /* ── tab label: name + close button ──────────────────────────── */
    GtkWidget *label_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    tab->tab_label = gtk_label_new("untitled");

    GtkWidget *close_btn = gtk_button_new_with_label("✕");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(close_btn), "tab-close-btn");
    gtk_button_set_relief(GTK_BUTTON(close_btn), GTK_RELIEF_NONE);
    gtk_widget_set_focus_on_click(close_btn, FALSE);

    gtk_box_pack_start(GTK_BOX(label_box), tab->tab_label, TRUE,  TRUE,  0);
    gtk_box_pack_start(GTK_BOX(label_box), close_btn,      FALSE, FALSE, 0);
    gtk_widget_show_all(label_box);

    /* store tab pointer on the page widget */
    g_object_set_data(G_OBJECT(tab->scroll), "editor-tab", tab);

    gint page_idx = gtk_notebook_append_page(
        GTK_NOTEBOOK(app->notebook), tab->scroll, label_box);
    gtk_notebook_set_tab_reorderable(
        GTK_NOTEBOOK(app->notebook), tab->scroll, TRUE);

    g_signal_connect(close_btn, "clicked",
        G_CALLBACK(on_tab_close_clicked), tab->scroll);

    if (path)
        tab_load_file(tab, path);

    gtk_notebook_set_current_page(GTK_NOTEBOOK(app->notebook), page_idx);
    gtk_widget_show_all(app->notebook);

    gtk_entry_set_text(GTK_ENTRY(app->filename_entry),
        path ? tab_display_name(tab) : "");

    /* grab focus after realization */
    g_idle_add((GSourceFunc)gtk_widget_grab_focus, tab->text_view);

    tab_update_line_numbers(tab);
    return tab;
}

/* ── tab close button ───────────────────────────────────────────────────── */

static void on_tab_close_clicked(GtkButton *btn, GtkWidget *page) {
    EditorTab *tab = g_object_get_data(G_OBJECT(page), "editor-tab");
    AppState  *app = tab->app;

    if (!tab_confirm_close(tab))
        return;

    gint n = gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook));
    if (n == 1) {
        /* last tab — clear instead of removing */
        tab->undo_inhibit = TRUE;
        gtk_text_buffer_set_text(tab->text_buf, "", -1);
        tab->undo_inhibit = FALSE;
        undo_stack_clear(&tab->undo_stack);
        undo_stack_clear(&tab->redo_stack);
        g_free(tab->current_file);
        tab->current_file = NULL;
        tab->is_dirty     = FALSE;
        gtk_label_set_text(GTK_LABEL(tab->tab_label), "untitled");
        gtk_entry_set_text(GTK_ENTRY(app->filename_entry), "");
        app_log(app, "▸ Tab cleared.");
    } else {
        gint idx = gtk_notebook_page_num(GTK_NOTEBOOK(app->notebook), page);
        gtk_notebook_remove_page(GTK_NOTEBOOK(app->notebook), idx);
        /* free the tab */
        undo_stack_free(&tab->undo_stack);
        undo_stack_free(&tab->redo_stack);
        g_free(tab->current_file);
        g_free(tab);
    }
}

/* ── notebook switch-page ───────────────────────────────────────────────── */

static void on_tab_switched(GtkNotebook *nb, GtkWidget *page,
                            guint idx, AppState *app) {
    EditorTab *tab = g_object_get_data(G_OBJECT(page), "editor-tab");
    if (!tab) return;
    gtk_entry_set_text(GTK_ENTRY(app->filename_entry),
        tab->current_file ? tab_display_name(tab) : "");
}

/* ── app_log ────────────────────────────────────────────────────────────── */

static void app_log(AppState *app, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char *msg = g_strdup_vprintf(fmt, args);
    va_end(args);

    GtkTextIter end;
    gtk_text_buffer_get_end_iter(app->log_buf, &end);
    gtk_text_buffer_insert(app->log_buf, &end, msg, -1);
    gtk_text_buffer_insert(app->log_buf, &end, "\n", -1);
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(app->log_view),
        &end, 0.0, FALSE, 0.0, 0.0);
    gtk_label_set_text(GTK_LABEL(app->status_label), msg);
    g_free(msg);
}

/* ── font size ──────────────────────────────────────────────────────────── */

static void app_apply_font_size(AppState *app) {
    char css[256];
    g_snprintf(css, sizeof(css),
        "#editor-view, #editor-view text { font-size: %dpx; }"
        "#line-numbers, #line-numbers text { font-size: %dpx; }",
        app->font_size, app->font_size);
    gtk_css_provider_load_from_data(app->size_provider, css, -1, NULL);
    app_log(app, "▸ Text size: %dpx", app->font_size);
}

static void on_text_size_inc(GtkMenuItem *item, AppState *app) {
    if (app->font_size < MAX_FONT_PX) {
        app->font_size += 2;
        app_apply_font_size(app);
    }
}

static void on_text_size_dec(GtkMenuItem *item, AppState *app) {
    if (app->font_size > MIN_FONT_PX) {
        app->font_size -= 2;
        app_apply_font_size(app);
    }
}

/* ── scroll zoom ────────────────────────────────────────────────────────── */

static gboolean on_scroll_zoom(GtkWidget *w, GdkEventScroll *ev, AppState *app) {
    if (!(ev->state & GDK_CONTROL_MASK))
        return FALSE;
    if (ev->direction == GDK_SCROLL_UP) {
        on_text_size_inc(NULL, app); return TRUE;
    }
    if (ev->direction == GDK_SCROLL_DOWN) {
        on_text_size_dec(NULL, app); return TRUE;
    }
    if (ev->direction == GDK_SCROLL_SMOOTH) {
        gdouble dx, dy;
        gdk_event_get_scroll_deltas((GdkEvent*)ev, &dx, &dy);
        if (dy < 0) on_text_size_inc(NULL, app);
        else if (dy > 0) on_text_size_dec(NULL, app);
        return TRUE;
    }
    return FALSE;
}

/* ── resolve path ───────────────────────────────────────────────────────── */

static char *resolve_path(const char *name) {
    if (!name || strlen(name) == 0)
        return NULL;
    if (g_path_is_absolute(name))
        return g_strdup(name);

    char *notes_dir = get_notes_dir();
    char *full = g_build_filename(notes_dir, name, NULL);
    g_free(notes_dir);

    /* if no extension and file doesn't already exist, add .txt */
    if (!strrchr(name, '.') && !g_file_test(full, G_FILE_TEST_EXISTS)) {
        char *with_ext = g_strconcat(full, ".txt", NULL);
        g_free(full);
        return with_ext;
    }
    return full;
}

/* ── file ops ───────────────────────────────────────────────────────────── */

static void on_new(GtkMenuItem *item, AppState *app) {
    EditorTab *tab = app_current_tab(app);
    if (!tab_confirm_close(tab)) return;

    tab->undo_inhibit = TRUE;
    gtk_text_buffer_set_text(tab->text_buf, "", -1);
    tab->undo_inhibit = FALSE;
    undo_stack_clear(&tab->undo_stack);
    undo_stack_clear(&tab->redo_stack);
    g_free(tab->current_file);
    tab->current_file = NULL;
    tab->is_dirty     = FALSE;
    gtk_label_set_text(GTK_LABEL(tab->tab_label), "untitled");
    gtk_entry_set_text(GTK_ENTRY(app->filename_entry), "");
    app_log(app, "▸ New note — enter filename and hit SAVE.");
    gtk_widget_grab_focus(app->filename_entry);
}

static void on_new_tab(GtkMenuItem *item, AppState *app) {
    tab_new(app, NULL);
    app_log(app, "▸ New tab.");
    gtk_widget_grab_focus(app->filename_entry);
}

static void on_load(GtkMenuItem *item, AppState *app) {
    GtkWidget *dlg = gtk_file_chooser_dialog_new(
        "Open Note", GTK_WINDOW(app->window),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open",   GTK_RESPONSE_ACCEPT,
        NULL);

    char *notes_dir = get_notes_dir();
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), notes_dir);
    g_free(notes_dir);

    /* file filters */
    GtkFileFilter *filt_all = gtk_file_filter_new();
    gtk_file_filter_set_name(filt_all, "All supported (txt, py, c, sh)");
    gtk_file_filter_add_pattern(filt_all, "*.txt");
    gtk_file_filter_add_pattern(filt_all, "*.py");
    gtk_file_filter_add_pattern(filt_all, "*.c");
    gtk_file_filter_add_pattern(filt_all, "*.h");
    gtk_file_filter_add_pattern(filt_all, "*.sh");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dlg), filt_all);

    GtkFileFilter *filt_any = gtk_file_filter_new();
    gtk_file_filter_set_name(filt_any, "All files (*)");
    gtk_file_filter_add_pattern(filt_any, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dlg), filt_any);

    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
        char *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        tab_new(app, path);
        g_free(path);
    }
    gtk_widget_destroy(dlg);
}

static void do_save(AppState *app, EditorTab *tab, const char *path) {
    char *spath = g_strdup(path); /* dup: path may point to tab->current_file */
    GtkTextIter s, e;
    gtk_text_buffer_get_bounds(tab->text_buf, &s, &e);
    char *text = gtk_text_buffer_get_text(tab->text_buf, &s, &e, FALSE);

    FILE *fp = fopen(spath, "w");
    if (!fp) {
        app_log(app, "▸ ERROR saving: %s", g_strerror(errno));
    } else {
        fputs(text, fp);
        fclose(fp);
        g_free(tab->current_file);
        tab->current_file = g_strdup(spath);
        tab_mark_clean(tab);
        tab_update_label(tab);
        char *dn = tab_display_name(tab);
        gtk_entry_set_text(GTK_ENTRY(app->filename_entry), dn);
        g_free(dn);
        app_log(app, "▸ Saved: %s", spath);
    }
    g_free(text);
    g_free(spath);
}

static void on_save(GtkMenuItem *item, AppState *app) {
    EditorTab *tab = app_current_tab(app);
    if (!tab) return;
    if (!tab->current_file) {
        on_save_as(item, app);
        return;
    }
    do_save(app, tab, tab->current_file);
}

static void on_save_as(GtkMenuItem *item, AppState *app) {
    EditorTab *tab = app_current_tab(app);
    if (!tab) return;

    GtkWidget *dlg = gtk_file_chooser_dialog_new(
        "Save As", GTK_WINDOW(app->window),
        GTK_FILE_CHOOSER_ACTION_SAVE,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Save",   GTK_RESPONSE_ACCEPT,
        NULL);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dlg), TRUE);

    char *notes_dir = get_notes_dir();
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg),
        tab->current_file ? g_path_get_dirname(tab->current_file) : notes_dir);
    g_free(notes_dir);

    const char *name = gtk_entry_get_text(GTK_ENTRY(app->filename_entry));
    if (name && strlen(name) > 0) {
        char *suggested = strchr(name, '.') ? g_strdup(name)
                                             : g_strconcat(name, ".txt", NULL);
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dlg), suggested);
        g_free(suggested);
    }

    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
        char *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        do_save(app, tab, path);
        g_free(path);
    }
    gtk_widget_destroy(dlg);
}

static void on_delete_file(GtkMenuItem *item, AppState *app) {
    EditorTab *tab = app_current_tab(app);
    if (!tab) return;

    const char *entry = gtk_entry_get_text(GTK_ENTRY(app->filename_entry));
    char *path = tab->current_file ? g_strdup(tab->current_file)
                                   : resolve_path(entry);
    if (!path || !g_file_test(path, G_FILE_TEST_EXISTS)) {
        app_log(app, "▸ ERROR: no file to delete.");
        g_free(path);
        return;
    }

    char *base = g_path_get_basename(path);
    GtkWidget *dlg = gtk_message_dialog_new(
        GTK_WINDOW(app->window), GTK_DIALOG_MODAL,
        GTK_MESSAGE_WARNING, GTK_BUTTONS_OK_CANCEL,
        "Delete %s?", base);
    gint resp = gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);

    if (resp == GTK_RESPONSE_OK) {
        char *fname = g_strdup(base);
        g_free(base);
        if (unlink(path) == 0) {
            on_new(NULL, app);
            app_log(app, "▸ Deleted: %s", fname);
        } else {
            app_log(app, "▸ ERROR deleting: %s", g_strerror(errno));
        }
        g_free(fname);
    } else {
        g_free(base);
    }
    g_free(path);
}

/* ── undo / redo ────────────────────────────────────────────────────────── */

static void on_undo(GtkMenuItem *item, AppState *app) {
    EditorTab *tab = app_current_tab(app);
    if (!tab || tab->undo_stack.count < 2) {
        app_log(app, "▸ Nothing to undo."); return;
    }
    /* pop current onto redo */
    char *current = g_strdup(undo_stack_top(&tab->undo_stack));
    tab->undo_stack.count--;
    undo_stack_push(&tab->redo_stack, current);
    g_free(current);

    const char *prev = undo_stack_top(&tab->undo_stack);
    tab->undo_inhibit = TRUE;
    gtk_text_buffer_set_text(tab->text_buf, prev, -1);
    tab->undo_inhibit = FALSE;
    app_log(app, "▸ Undo");
}

static void on_redo(GtkMenuItem *item, AppState *app) {
    EditorTab *tab = app_current_tab(app);
    if (!tab || tab->redo_stack.count == 0) {
        app_log(app, "▸ Nothing to redo."); return;
    }
    char *state = g_strdup(undo_stack_top(&tab->redo_stack));
    tab->redo_stack.count--;
    undo_stack_push(&tab->undo_stack, state);
    tab->undo_inhibit = TRUE;
    gtk_text_buffer_set_text(tab->text_buf, state, -1);
    tab->undo_inhibit = FALSE;
    g_free(state);
    app_log(app, "▸ Redo");
}

/* ── key press handler ──────────────────────────────────────────────────── */

static gboolean on_key_press(GtkWidget *w, GdkEventKey *ev, AppState *app) {
    gboolean ctrl  = ev->state & GDK_CONTROL_MASK;
    gboolean shift = ev->state & GDK_SHIFT_MASK;
    EditorTab *tab = app_current_tab(app);
    if (!tab) return FALSE;
    GtkTextBuffer *buf = tab->text_buf;

    /* Ctrl+S — save */
    if (ctrl && ev->keyval == GDK_KEY_s) {
        on_save(NULL, app); return TRUE;
    }
    /* Ctrl+Z — undo */
    if (ctrl && ev->keyval == GDK_KEY_z) {
        on_undo(NULL, app); return TRUE;
    }
    /* Ctrl+Y — redo */
    if (ctrl && ev->keyval == GDK_KEY_y) {
        on_redo(NULL, app); return TRUE;
    }
    /* Ctrl+T — new tab */
    if (ctrl && ev->keyval == GDK_KEY_t) {
        tab_new(app, NULL);
        app_log(app, "▸ New tab.");
        gtk_widget_grab_focus(app->filename_entry);
        return TRUE;
    }
    if (ctrl && ev->keyval == GDK_KEY_slash) {
        toggle_comment(app); return TRUE;
    }
    if (ctrl && ev->keyval == GDK_KEY_d) {
        duplicate_line(app); return TRUE;
    }
    /* Ctrl++ / Ctrl+= — font up */
    if (ctrl && (ev->keyval == GDK_KEY_plus  ||
                 ev->keyval == GDK_KEY_equal  ||
                 ev->keyval == GDK_KEY_KP_Add)) {
        on_text_size_inc(NULL, app); return TRUE;
    }
    /* Ctrl+- — font down */
    if (ctrl && (ev->keyval == GDK_KEY_minus ||
                 ev->keyval == GDK_KEY_KP_Subtract)) {
        on_text_size_dec(NULL, app); return TRUE;
    }

    /* Tab / Shift+Tab — indent / dedent selection */
    if (ev->keyval == GDK_KEY_Tab ||
        (shift && ev->keyval == GDK_KEY_ISO_Left_Tab)) {
        GtkTextIter sel_start, sel_end;
        gboolean has_sel = gtk_text_buffer_get_selection_bounds(
            buf, &sel_start, &sel_end);

        if (has_sel) {
            gint start_line = gtk_text_iter_get_line(&sel_start);
            gint end_line   = gtk_text_iter_get_line(&sel_end);
            if (gtk_text_iter_get_line_offset(&sel_end) == 0 &&
                end_line > start_line)
                end_line--;

            gtk_text_buffer_begin_user_action(buf);
            for (gint line = start_line; line <= end_line; line++) {
                GtkTextIter it;
                gtk_text_buffer_get_iter_at_line(buf, &it, line);
                if (shift) {
                    /* dedent: remove up to 4 leading spaces */
                    for (int r = 0; r < 4; r++) {
                        GtkTextIter next = it;
                        if (!gtk_text_iter_forward_char(&next)) break;
                        char *ch = gtk_text_buffer_get_text(buf, &it, &next, FALSE);
                        if (strcmp(ch, " ") == 0) {
                            gtk_text_buffer_delete(buf, &it, &next);
                            gtk_text_buffer_get_iter_at_line(buf, &it, line);
                        } else {
                            g_free(ch);
                            break;
                        }
                        g_free(ch);
                    }
                } else {
                    gtk_text_buffer_insert(buf, &it, "    ", -1);
                }
            }
            gtk_text_buffer_end_user_action(buf);
        } else if (!shift) {
            gtk_text_buffer_insert_at_cursor(buf, "    ", -1);
        }
        return TRUE;
    }

    /* auto-close brackets & quotes */
    if (!ctrl) {
        const char *open_ch = NULL, *close_ch = NULL;
        switch (ev->keyval) {
            case GDK_KEY_parenleft:   open_ch = "("; close_ch = ")"; break;
            case GDK_KEY_bracketleft: open_ch = "["; close_ch = "]"; break;
            case GDK_KEY_braceleft:   open_ch = "{"; close_ch = "}"; break;
            case GDK_KEY_apostrophe:  open_ch = "'"; close_ch = "'"; break;
            case GDK_KEY_quotedbl:    open_ch = "\""; close_ch = "\""; break;
        }
        if (open_ch) {
            GtkTextIter cursor;
            gtk_text_buffer_get_iter_at_mark(buf, &cursor,
                gtk_text_buffer_get_insert(buf));
            /* skip over existing closer */
            GtkTextIter next = cursor;
            if (gtk_text_iter_forward_char(&next)) {
                char *ch = gtk_text_buffer_get_text(buf, &cursor, &next, FALSE);
                if (strcmp(ch, close_ch) == 0) {
                    gtk_text_buffer_place_cursor(buf, &next);
                    g_free(ch);
                    return TRUE;
                }
                g_free(ch);
            }
            /* insert both, move cursor between them */
            char pair[4];
            g_snprintf(pair, sizeof(pair), "%s%s", open_ch, close_ch);
            gtk_text_buffer_insert_at_cursor(buf, pair, -1);
            GtkTextIter back;
            gtk_text_buffer_get_iter_at_mark(buf, &back,
                gtk_text_buffer_get_insert(buf));
            gtk_text_iter_backward_char(&back);
            gtk_text_buffer_place_cursor(buf, &back);
            return TRUE;
        }
    }

    /* backspace — delete auto-closed pair */
    if (ev->keyval == GDK_KEY_BackSpace) {
        GtkTextIter cursor;
        gtk_text_buffer_get_iter_at_mark(buf, &cursor,
            gtk_text_buffer_get_insert(buf));
        GtkTextIter prev = cursor;
        if (gtk_text_iter_backward_char(&prev)) {
            GtkTextIter next = cursor;
            if (gtk_text_iter_forward_char(&next)) {
                char *left  = gtk_text_buffer_get_text(buf, &prev, &cursor, FALSE);
                char *right = gtk_text_buffer_get_text(buf, &cursor, &next, FALSE);
                gboolean is_pair =
                    (strcmp(left,"(")  == 0 && strcmp(right,")") == 0) ||
                    (strcmp(left,"[")  == 0 && strcmp(right,"]") == 0) ||
                    (strcmp(left,"{")  == 0 && strcmp(right,"}") == 0) ||
                    (strcmp(left,"'")  == 0 && strcmp(right,"'") == 0) ||
                    (strcmp(left,"\"") == 0 && strcmp(right,"\"") == 0);
                if (is_pair) {
                    gtk_text_buffer_delete(buf, &prev, &next);
                    g_free(left); g_free(right);
                    return TRUE;
                }
                g_free(left); g_free(right);
            }
        }
    }

    /* Enter — smart indent */
    if (ev->keyval == GDK_KEY_Return || ev->keyval == GDK_KEY_KP_Enter) {
        GtkTextIter cursor;
        gtk_text_buffer_get_iter_at_mark(buf, &cursor,
            gtk_text_buffer_get_insert(buf));
        GtkTextIter line_start = cursor;
        gtk_text_iter_set_line_offset(&line_start, 0);
        char *line_text = gtk_text_buffer_get_text(buf, &line_start, &cursor, FALSE);

        /* count leading spaces */
        int base_indent = 0;
        while (line_text[base_indent] == ' ') base_indent++;

        /* check for indent trigger */
        const char *stripped = line_text + base_indent;
        static const char *triggers[] = {
            "if", "elif", "else", "except", "def", "for", "while",
            "with", "try", "finally", "class", NULL
        };
        gboolean add_indent = FALSE;
        size_t slen = strlen(stripped);
        if (slen > 0 && stripped[slen-1] == ':') {
            char first_word[32] = {0};
            sscanf(stripped, "%31s", first_word);
            /* strip trailing ( from word */
            char *paren = strchr(first_word, '(');
            if (paren) *paren = '\0';
            for (int i = 0; triggers[i]; i++) {
                if (strcmp(first_word, triggers[i]) == 0) {
                    add_indent = TRUE; break;
                }
            }
        }
        g_free(line_text);

        int total = base_indent + (add_indent ? 4 : 0);
        char *indent = g_strnfill(total + 1, ' ');
        indent[0] = '\n';
        for (int i = 1; i <= total; i++) indent[i] = ' ';
        indent[total + 1] = '\0';  /* wait, g_strnfill NUL-terminates */

        /* build "\n" + spaces */
        GString *ins = g_string_new("\n");
        for (int i = 0; i < total; i++) g_string_append_c(ins, ' ');
        gtk_text_buffer_insert_at_cursor(buf, ins->str, -1);
        g_string_free(ins, TRUE);
        g_free(indent);
        return TRUE;
    }

    return FALSE;
}

/* ── FILE menu ──────────────────────────────────────────────────────────── */

static void on_file_menu_clicked(GtkButton *btn, AppState *app) {
    gtk_menu_popup_at_widget(GTK_MENU(app->file_menu),
        GTK_WIDGET(btn),
        GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST,
        NULL);
}

static GtkWidget *make_menu_item(const char *label,
                                  GCallback cb, gpointer data) {
    GtkWidget *item = gtk_menu_item_new_with_label(label);
    if (cb) g_signal_connect(item, "activate", cb, data);
    return item;
}


/* ── syntax highlighting ─────────────────────────────────────────────────── */

/* Default config written to cyon_notes.conf if sections are absent */
static const char *HL_DEFAULT_CONF =
    "[highlight_group_1]\n"
    "keywords = for while if elif else with break continue pass yield raise assert printf g_print snprintf system echo exit return\n"
    "color = #ffb000\n"
    "\n"
    "[highlight_group_2]\n"
    "keywords = def class import from as lambda global nonlocal del expanduser join append extend insert pop update get items keys values function source export alias unset\n"
    "color = #5fd7ff\n"
    "\n"
    "[highlight_group_3]\n"
    "keywords = try except finally self super\n"
    "color = #8fd3ff\n"
    "\n"
    "[highlight_group_4]\n"
    "keywords = True False None NotImplemented Ellipsis sizeof NULL nullptr true false\n"
    "color = #ff4444\n"
    "\n"
    "[highlight_group_5]\n"
    "keywords = display position margin padding border background width height font font-family font-size font-weight align justify flex grid overflow transition transform opacity cursor\n"
    "color = #ff88ff\n"
    "\n"
    "[highlight_group_6]\n"
    "keywords = len range open map filter list dict set tuple int float str bool type isinstance enumerate zip sorted reversed sum min max abs round hasattr getattr setattr callable gboolean gint gchar GtkWidget GtkWindow GtkBox GtkButton GtkLabel\n"
    "color = #00ffaa\n"
    "\n"
    "[highlight_group_7]\n"
    "keywords = and or not in is\n"
    "color = #ffd700\n"
    "\n"
    "[highlight_group_8]\n"
    "keywords = os sys subprocess pathlib read write mkdir remove rename chmod chown stat glob shutil copy move cd ls cat grep sed awk find ps kill sudo apt git\n"
    "color = #00eaff\n"
    "\n"
    "[highlight_group_9]\n"
    "keywords = eval exec compile __import__ malloc free calloc realloc goto\n"
    "color = #ff5555\n"
    "\n"
    "[highlight_group_10]\n"
    "keywords = breakpoint help dir vars globals locals __name__ __file__ __doc__ __all__ __init__ __main__\n"
    "color = #c792ea\n"
    "\n"
    "[highlight_group_11]\n"
    "keywords = void static const char unsigned signed long short struct union enum typedef extern inline auto register volatile restrict\n"
    "color = #8fd3ff\n"
    "\n"
    "[highlight_group_12]\n"
    "keywords = include define ifdef ifndef endif pragma undef\n"
    "color = #ffb347\n"
    "\n"
    "[highlight_special]\n"
    "color_lime    = #c8ff00\n"
    "color_coral   = #ff9966\n"
    "color_comment = #6a5acd\n"
    "color_dot_left  = #ffffff\n"
    "color_dot_right = #ffb000\n"
    "color_steel   = #8fd3ff\n";

static char *get_config_path(void) {
    /* same directory as the executable */
    char exe_path[4096] = {0};
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len > 0) {
        exe_path[len] = '\0';
        char *dir = g_path_get_dirname(exe_path);
        char *cfg = g_build_filename(dir, CONFIG_FILENAME, NULL);
        g_free(dir);
        return cfg;
    }
    return g_strdup(CONFIG_FILENAME);
}

static void hl_ensure_config(const char *path) {
    /* if file doesn't exist, write defaults */
    if (g_file_test(path, G_FILE_TEST_EXISTS))
        return;
    GError *err = NULL;
    if (!g_file_set_contents(path, HL_DEFAULT_CONF, -1, &err)) {
        g_printerr("cyon_notes: could not write config: %s\n", err->message);
        g_error_free(err);
    }
}

static GtkTextTag *make_tag(GtkTextBuffer *buf, const char *color) {
    /* Use a shared tag table on a dummy buffer so all tab buffers can share
     * the same tag objects via the tag table. For simplicity we create tags
     * on a provided buffer — each tab buffer gets its own set. Since we only
     * call hl_apply from on_text_changed (which passes the current tab's
     * buffer), we store tags on AppState and they apply to any buffer. */
    return gtk_text_buffer_create_tag(buf, NULL, "foreground", color, NULL);
}

/* We need one canonical buffer to own the tags. We use a hidden buffer. */

static void hl_load_config(AppState *app) {
    char *path = get_config_path();
    hl_ensure_config(path);

    GKeyFile *kf   = g_key_file_new();
    GError   *err  = NULL;
    g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, &err);
    if (err) {
        g_printerr("cyon_notes: config load error: %s\n", err->message);
        g_error_free(err);
        /* fall back to loading from the default string */
        g_key_file_load_from_data(kf, HL_DEFAULT_CONF, -1,
                                   G_KEY_FILE_NONE, NULL);
    }
    g_free(path);

    /* tag owner buffer — never displayed, just holds tag table */
    if (!g_shared_tags)
        g_shared_tags = gtk_text_tag_table_new();

    /* free any existing tags/regex from a previous load */
    if (app->hl_kw_tags) {
        for (int i = 0; i < app->hl_group_count; i++) {
            if (app->hl_kw_regex[i]) g_regex_unref(app->hl_kw_regex[i]);
            if (app->hl_kw_tags[i])  gtk_text_tag_table_remove(g_shared_tags, app->hl_kw_tags[i]);
        }
        g_free(app->hl_kw_tags);
        g_free(app->hl_kw_regex);
        app->hl_kw_tags  = NULL;
        app->hl_kw_regex = NULL;
    }
    /* remove special tags too */
    GtkTextTag *old_specials[] = {
        app->hl_tag_lime, app->hl_tag_coral, app->hl_tag_comment,
        app->hl_tag_dot_l, app->hl_tag_dot_r, app->hl_tag_steel, NULL
    };
    for (int i = 0; old_specials[i]; i++)
        gtk_text_tag_table_remove(g_shared_tags, old_specials[i]);

    /* collect all highlight_group_N sections */
    gsize    n_groups = 0;
    gchar  **groups   = g_key_file_get_groups(kf, NULL);
    /* count matching sections */
    for (int i = 0; groups[i]; i++) {
        if (g_str_has_prefix(groups[i], "highlight_group_"))
            n_groups++;
    }

    app->hl_group_count = (int)n_groups;
    app->hl_kw_tags     = g_new0(GtkTextTag *, n_groups);
    app->hl_kw_regex    = g_new0(GRegex *,     n_groups);

    int gi = 0;
    /* iterate in sorted order: highlight_group_1 .. highlight_group_N */
    for (int n = 1; n <= 99 && gi < (int)n_groups; n++) {
        char sec[64];
        g_snprintf(sec, sizeof(sec), "highlight_group_%d", n);
        if (!g_key_file_has_group(kf, sec))
            continue;

        char *color    = g_key_file_get_string(kf, sec, "color",    NULL);
        char *kw_str   = g_key_file_get_string(kf, sec, "keywords", NULL);
        if (!color || !kw_str) {
            g_free(color); g_free(kw_str);
            gi++;
            continue;
        }

        /* build regex: \b(kw1|kw2|...)\b */
        gchar  **words   = g_strsplit_set(kw_str, " \t", -1);
        GString *pattern = g_string_new("\\b(");
        gboolean first   = TRUE;
        for (int w = 0; words[w]; w++) {
            if (!words[w][0]) continue;
            if (!first) g_string_append_c(pattern, '|');
            char *esc = g_regex_escape_string(words[w], -1);
            g_string_append(pattern, esc);
            g_free(esc);
            first = FALSE;
        }
        g_string_append(pattern, ")(?!\\w)");

        GError *rerr = NULL;
        app->hl_kw_regex[gi] = g_regex_new(pattern->str,
            G_REGEX_OPTIMIZE, 0, &rerr);
        if (rerr) {
            g_printerr("cyon_notes: regex error group %d: %s\n",
                n, rerr->message);
            g_error_free(rerr);
            app->hl_kw_regex[gi] = NULL;
        }
        app->hl_kw_tags[gi] = gtk_text_tag_new(NULL);
        g_object_set(app->hl_kw_tags[gi], "foreground", color, NULL);
        gtk_text_tag_table_add(g_shared_tags, app->hl_kw_tags[gi]);

        g_string_free(pattern, TRUE);
        g_strfreev(words);
        g_free(color);
        g_free(kw_str);
        gi++;
    }
    g_strfreev(groups);

    /* special tags */
    char *col_lime    = g_key_file_get_string(kf, "highlight_special", "color_lime",    NULL);
    char *col_coral   = g_key_file_get_string(kf, "highlight_special", "color_coral",   NULL);
    char *col_comment = g_key_file_get_string(kf, "highlight_special", "color_comment", NULL);
    char *col_dot_l   = g_key_file_get_string(kf, "highlight_special", "color_dot_left",  NULL);
    char *col_dot_r   = g_key_file_get_string(kf, "highlight_special", "color_dot_right", NULL);
    char *col_steel   = g_key_file_get_string(kf, "highlight_special", "color_steel",   NULL);

    app->hl_tag_lime    = gtk_text_tag_new(NULL);
    g_object_set(app->hl_tag_lime,    "foreground", col_lime    ? col_lime    : "#c8ff00", NULL);
    gtk_text_tag_table_add(g_shared_tags, app->hl_tag_lime);
    app->hl_tag_coral   = gtk_text_tag_new(NULL);
    g_object_set(app->hl_tag_coral,   "foreground", col_coral   ? col_coral   : "#ff9966", NULL);
    gtk_text_tag_table_add(g_shared_tags, app->hl_tag_coral);
    app->hl_tag_comment = gtk_text_tag_new(NULL);
    g_object_set(app->hl_tag_comment, "foreground", col_comment ? col_comment : "#6a5acd", NULL);
    gtk_text_tag_table_add(g_shared_tags, app->hl_tag_comment);
    app->hl_tag_dot_l   = gtk_text_tag_new(NULL);
    g_object_set(app->hl_tag_dot_l,   "foreground", col_dot_l   ? col_dot_l   : "#ffffff", NULL);
    gtk_text_tag_table_add(g_shared_tags, app->hl_tag_dot_l);
    app->hl_tag_dot_r   = gtk_text_tag_new(NULL);
    g_object_set(app->hl_tag_dot_r,   "foreground", col_dot_r   ? col_dot_r   : "#ffb000", NULL);
    gtk_text_tag_table_add(g_shared_tags, app->hl_tag_dot_r);
    app->hl_tag_steel   = gtk_text_tag_new(NULL);
    g_object_set(app->hl_tag_steel,   "foreground", col_steel   ? col_steel   : "#8fd3ff", NULL);
    gtk_text_tag_table_add(g_shared_tags, app->hl_tag_steel);

    g_free(col_lime); g_free(col_coral); g_free(col_comment);
    g_free(col_dot_l); g_free(col_dot_r); g_free(col_steel);
    g_key_file_free(kf);
}

/* Apply a tag to all matches of a GRegex in text, using the given buffer */
static void hl_apply_regex(GtkTextBuffer *buf, GRegex *re,
                            GtkTextTag *tag, const char *text,
                            int group) {
    if (!re || !tag) return;
    GMatchInfo *mi = NULL;
    g_regex_match(re, text, 0, &mi);
    while (g_match_info_matches(mi)) {
        gint s, e;
        g_match_info_fetch_pos(mi, group, &s, &e);
        if (s >= 0 && e > s) {
            /* fetch_pos returns byte offsets; convert to char offsets for GTK */
            glong cs = g_utf8_pointer_to_offset(text, text + s);
            glong ce = g_utf8_pointer_to_offset(text, text + e);
            GtkTextIter si, ei;
            gtk_text_buffer_get_iter_at_offset(buf, &si, (gint)cs);
            gtk_text_buffer_get_iter_at_offset(buf, &ei, (gint)ce);
            gtk_text_buffer_apply_tag(buf, tag, &si, &ei);
        }
        g_match_info_next(mi, NULL);
    }
    g_match_info_free(mi);
}

static void hl_apply(AppState *app, GtkTextBuffer *buf) {
    if (!app->hl_tag_comment) return;  /* not loaded yet */

    GtkTextIter s, e;
    gtk_text_buffer_get_bounds(buf, &s, &e);

    /* clear all tags first */
    for (int i = 0; i < app->hl_group_count; i++)
        if (app->hl_kw_tags[i])
            gtk_text_buffer_remove_tag(buf, app->hl_kw_tags[i], &s, &e);

    if (app->hl_tag_lime)    gtk_text_buffer_remove_tag(buf, app->hl_tag_lime,    &s, &e);
    if (app->hl_tag_coral)   gtk_text_buffer_remove_tag(buf, app->hl_tag_coral,   &s, &e);
    if (app->hl_tag_comment) gtk_text_buffer_remove_tag(buf, app->hl_tag_comment, &s, &e);
    if (app->hl_tag_dot_l)   gtk_text_buffer_remove_tag(buf, app->hl_tag_dot_l,   &s, &e);
    if (app->hl_tag_dot_r)   gtk_text_buffer_remove_tag(buf, app->hl_tag_dot_r,   &s, &e);
    if (app->hl_tag_steel)   gtk_text_buffer_remove_tag(buf, app->hl_tag_steel,   &s, &e);

    char *text = gtk_text_buffer_get_text(buf, &s, &e, FALSE);

    /* 1) keyword groups — lowest priority */
    for (int i = 0; i < app->hl_group_count; i++)
        hl_apply_regex(buf, app->hl_kw_regex[i], app->hl_kw_tags[i], text, 1);

    /* 2) steel: standalone = operator */
    {
        static GRegex *re_eq = NULL;
        if (!re_eq) re_eq = g_regex_new("(?<![=!<>])=(?!=)", G_REGEX_OPTIMIZE, 0, NULL);
        hl_apply_regex(buf, re_eq, app->hl_tag_steel, text, 0);
    }

    /* 3) steel: @staticmethod */
    {
        static GRegex *re_sm = NULL;
        if (!re_sm) re_sm = g_regex_new("@staticmethod(?!\\w)", G_REGEX_OPTIMIZE, 0, NULL);
        hl_apply_regex(buf, re_sm, app->hl_tag_steel, text, 0);
    }

    /* 4) lime: class name after 'class' */
    {
        static GRegex *re_cls = NULL;
        if (!re_cls) re_cls = g_regex_new("(?<!\\w)class\\s+(\\w+)", G_REGEX_OPTIMIZE, 0, NULL);
        hl_apply_regex(buf, re_cls, app->hl_tag_lime, text, 1);
    }

    /* 5) dot notation: obj.method */
    {
        static GRegex *re_dot = NULL;
        if (!re_dot) re_dot = g_regex_new("(?<!\\w)(\\w+)\\.(\\w+)(?!\\w)", G_REGEX_OPTIMIZE, 0, NULL);
        GMatchInfo *mi = NULL;
        g_regex_match(re_dot, text, 0, &mi);
        while (g_match_info_matches(mi)) {
            gint ls, le, rs, re2;
            g_match_info_fetch_pos(mi, 1, &ls, &le);
            g_match_info_fetch_pos(mi, 2, &rs, &re2);

            if (ls >= 0) {
                GtkTextIter a, b;
                glong cls = g_utf8_pointer_to_offset(text, text + ls);
                glong cle = g_utf8_pointer_to_offset(text, text + le);
                gtk_text_buffer_get_iter_at_offset(buf, &a, (gint)cls);
                gtk_text_buffer_get_iter_at_offset(buf, &b, (gint)cle);
                gtk_text_buffer_apply_tag(buf, app->hl_tag_dot_l, &a, &b);
            }
            if (rs >= 0) {
                GtkTextIter a, b;
                glong crs = g_utf8_pointer_to_offset(text, text + rs);
                glong cre = g_utf8_pointer_to_offset(text, text + re2);
                gtk_text_buffer_get_iter_at_offset(buf, &a, (gint)crs);
                gtk_text_buffer_get_iter_at_offset(buf, &b, (gint)cre);
                gtk_text_buffer_apply_tag(buf, app->hl_tag_dot_r, &a, &b);
            }
            g_match_info_next(mi, NULL);
        }
        g_match_info_free(mi);
    }

    /* 6) strings — high priority, paint over keywords but under comments */
    {
        static GRegex *re_str = NULL;
        if (!re_str) re_str = g_regex_new("(['\"])(.*?)\\1", G_REGEX_OPTIMIZE|G_REGEX_DOTALL, 0, NULL);
        GMatchInfo *mi = NULL;
        g_regex_match(re_str, text, 0, &mi);
        while (g_match_info_matches(mi)) {
            gint cs, ce;
            g_match_info_fetch_pos(mi, 2, &cs, &ce);
            if (cs >= 0 && ce > cs) {
                GtkTextIter si, ei;
                glong ccs = g_utf8_pointer_to_offset(text, text + cs);
                glong cce = g_utf8_pointer_to_offset(text, text + ce);
                gtk_text_buffer_get_iter_at_offset(buf, &si, (gint)ccs);
                gtk_text_buffer_get_iter_at_offset(buf, &ei, (gint)cce);
                gtk_text_buffer_apply_tag(buf, app->hl_tag_coral, &si, &ei);
            }
            g_match_info_next(mi, NULL);
        }
        g_match_info_free(mi);
    }

    /* 7) comments — highest priority, paint last */
    {
        static GRegex *re_cmt = NULL;
        if (!re_cmt) re_cmt = g_regex_new("(#[^\n]*|//[^\n]*)", G_REGEX_OPTIMIZE, 0, NULL);
        hl_apply_regex(buf, re_cmt, app->hl_tag_comment, text, 0);
    }

    g_free(text);
}
/* ── destroy ────────────────────────────────────────────────────────────── */


/* ══════════════════════════════════════════════════════════════════════════
 * FEATURE: cursor position in status bar
 * ══════════════════════════════════════════════════════════════════════════ */

static void update_cursor_label(AppState *app) {
    EditorTab *tab = app_current_tab(app);
    if (!tab || !app->cursor_label) return;
    GtkTextIter it;
    gtk_text_buffer_get_iter_at_mark(tab->text_buf, &it,
        gtk_text_buffer_get_insert(tab->text_buf));
    int line = gtk_text_iter_get_line(&it) + 1;
    int col  = gtk_text_iter_get_line_offset(&it) + 1;
    char buf[64];
    g_snprintf(buf, sizeof(buf), "Ln %d  Col %d", line, col);
    gtk_label_set_text(GTK_LABEL(app->cursor_label), buf);
}

static void on_cursor_moved(GtkTextBuffer *buf, GtkTextIter *loc,
                             GtkTextMark *mark, AppState *app) {
    if (mark == gtk_text_buffer_get_insert(buf))
        update_cursor_label(app);
}

/* ══════════════════════════════════════════════════════════════════════════
 * FEATURE: dirty dot on tab label
 * ══════════════════════════════════════════════════════════════════════════ */

/* Called from tab_update_label — already declared, just needs dirty check */
/* We hook this by overriding tab_update_label to append ● when dirty */



/* ══════════════════════════════════════════════════════════════════════════
 * FEATURE: TTS  (piper → aplay)
 * ══════════════════════════════════════════════════════════════════════════ */

static void tts_run(AppState *app, const char *text) {
    if (!text || !text[0]) return;

    /* kill any running TTS process */
    if (app->tts_pid) {
        kill(app->tts_pid, SIGTERM);
        g_spawn_close_pid(app->tts_pid);
        app->tts_pid = 0;
    }

    /* piper executable path */
    const char *piper_path = "/home/cruxible/pyra_env/bin/piper";

    /* choose voice model and make full path */
    char voice_model[PATH_MAX];
    snprintf(voice_model, sizeof(voice_model),
             "/home/cruxible/cyon/piper_models/%s",
             app->tts_voice_joe ? "en_US-joe-medium.onnx" : "en_US-lessac-medium.onnx");

    /* escape the text for shell */
    char *safe_text = g_shell_quote(text);

    /* build the command */
    char *cmd = g_strdup_printf(
        "echo %s | %s --model %s --output-raw | aplay -r 22050 -f S16_LE -c 1",
        safe_text, piper_path, voice_model);

    g_free(safe_text);

    /* spawn the TTS process */
    GError *err = NULL;
    gchar *argv[] = { "/bin/sh", "-c", cmd, NULL };
    if (!g_spawn_async(NULL, argv, NULL,
                       G_SPAWN_DEFAULT | G_SPAWN_DO_NOT_REAP_CHILD,
                       NULL, NULL, &app->tts_pid, &err)) {
        if (err) {
            app_log(app, "▸ TTS error: %s", err->message);
            g_error_free(err);
            app->tts_pid = 0;
        }
    } else {
        app_log(app, "▸ Speaking (%s)…",
                app->tts_voice_joe ? "Joe" : "Lessac");
    }

    g_free(cmd);
}

static void on_tts_speak_selection(GtkMenuItem *item, AppState *app) {
    EditorTab *tab = app_current_tab(app);
    if (!tab) return;
    GtkTextIter ss, se;
    if (!gtk_text_buffer_get_selection_bounds(tab->text_buf, &ss, &se)) {
        app_log(app, "▸ TTS: no selection."); return;
    }
    char *text = gtk_text_buffer_get_text(tab->text_buf, &ss, &se, FALSE);
    tts_run(app, text);
    g_free(text);
}

static void on_tts_speak_all(GtkMenuItem *item, AppState *app) {
    EditorTab *tab = app_current_tab(app);
    if (!tab) return;
    GtkTextIter s, e;
    gtk_text_buffer_get_bounds(tab->text_buf, &s, &e);
    char *text = gtk_text_buffer_get_text(tab->text_buf, &s, &e, FALSE);
    tts_run(app, text);
    g_free(text);
}

static void on_tts_stop(GtkMenuItem *item, AppState *app) {
    if (app->tts_pid) {
        kill(app->tts_pid, SIGTERM);
        g_spawn_close_pid(app->tts_pid);
        app->tts_pid = 0;
        app_log(app, "▸ TTS stopped.");
    }
}

static void on_tts_voice_joe(GtkMenuItem *item, AppState *app) {
    app->tts_voice_joe = TRUE;
    app_log(app, "▸ Voice: Joe (male).");
}

static void on_tts_voice_lessac(GtkMenuItem *item, AppState *app) {
    app->tts_voice_joe = FALSE;
    app_log(app, "▸ Voice: Lessac (female).");
}

/* ══════════════════════════════════════════════════════════════════════════
 * FEATURE: Ctrl+/ toggle comment   Ctrl+D duplicate line
 * ══════════════════════════════════════════════════════════════════════════ */

static void toggle_comment(AppState *app) {
    EditorTab *tab = app_current_tab(app);
    if (!tab) return;
    GtkTextBuffer *buf = tab->text_buf;

    GtkTextIter sel_s, sel_e;
    gboolean has_sel = gtk_text_buffer_get_selection_bounds(
        buf, &sel_s, &sel_e);
    gint start_line = gtk_text_iter_get_line(&sel_s);
    gint end_line   = has_sel ? gtk_text_iter_get_line(&sel_e) : start_line;
    if (has_sel && gtk_text_iter_get_line_offset(&sel_e) == 0
        && end_line > start_line)
        end_line--;

    /* detect: if ALL lines start with #, uncomment; else comment */
    gboolean all_commented = TRUE;
    for (gint l = start_line; l <= end_line && all_commented; l++) {
        GtkTextIter it;
        gtk_text_buffer_get_iter_at_line(buf, &it, l);
        /* skip leading spaces */
        while (!gtk_text_iter_ends_line(&it) &&
               gtk_text_iter_get_char(&it) == ' ')
            gtk_text_iter_forward_char(&it);
        if (gtk_text_iter_get_char(&it) != '#')
            all_commented = FALSE;
    }

    gtk_text_buffer_begin_user_action(buf);
    for (gint l = start_line; l <= end_line; l++) {
        GtkTextIter it;
        gtk_text_buffer_get_iter_at_line(buf, &it, l);
        if (all_commented) {
            /* find and remove the # */
            while (!gtk_text_iter_ends_line(&it) &&
                   gtk_text_iter_get_char(&it) == ' ')
                gtk_text_iter_forward_char(&it);
            if (gtk_text_iter_get_char(&it) == '#') {
                GtkTextIter nx = it;
                gtk_text_iter_forward_char(&nx);
                /* also eat one space after # if present */
                if (gtk_text_iter_get_char(&nx) == ' ') {
                    GtkTextIter nx2 = nx;
                    gtk_text_iter_forward_char(&nx2);
                    gtk_text_buffer_delete(buf, &it, &nx2);
                } else {
                    gtk_text_buffer_delete(buf, &it, &nx);
                }
            }
        } else {
            gtk_text_buffer_insert(buf, &it, "# ", -1);
        }
    }
    gtk_text_buffer_end_user_action(buf);
}

static void duplicate_line(AppState *app) {
    EditorTab *tab = app_current_tab(app);
    if (!tab) return;
    GtkTextBuffer *buf = tab->text_buf;
    GtkTextIter cursor;
    gtk_text_buffer_get_iter_at_mark(buf, &cursor,
        gtk_text_buffer_get_insert(buf));
    gint line = gtk_text_iter_get_line(&cursor);

    GtkTextIter ls, le;
    gtk_text_buffer_get_iter_at_line(buf, &ls, line);
    le = ls;
    if (!gtk_text_iter_ends_line(&le))
        gtk_text_iter_forward_to_line_end(&le);
    char *text = gtk_text_buffer_get_text(buf, &ls, &le, FALSE);

    gtk_text_buffer_begin_user_action(buf);
    gtk_text_buffer_insert(buf, &le, "\n", -1);
    /* re-fetch iter after insert */
    gtk_text_buffer_get_iter_at_line(buf, &le, line);
    gtk_text_iter_forward_to_line_end(&le);
    gtk_text_iter_forward_char(&le);
    gtk_text_buffer_insert(buf, &le, text, -1);
    gtk_text_buffer_end_user_action(buf);
    g_free(text);
}

static void on_destroy(GtkWidget *w, AppState *app) {
    if (app->tts_pid) {
        kill(app->tts_pid, SIGTERM);
        g_spawn_close_pid(app->tts_pid);
    }
    g_free(app->tree_folder);
    release_lock();
    gtk_main_quit();
}

/* ── build UI ───────────────────────────────────────────────────────────── */

/* ── file tree ──────────────────────────────────────────────────────────── */

typedef struct {
    char *name;
    char *path;
} TreeItem;

static gint tree_item_compare(TreeItem *a, TreeItem *b) {
    return g_utf8_collate(a->name, b->name);
}

/* forward declaration for the folder monitor callback */

/* recursively add children to a tree node */
static void tree_add_children(AppState *app, GtkTreeIter *parent, const char *folder) {
    GDir *dir = g_dir_open(folder, 0, NULL);
    if (!dir) return;

    GList *dirs = NULL, *files = NULL;
    const char *name;

    while ((name = g_dir_read_name(dir))) {
        if (name[0] == '.') continue;

        /* Convert name safely to UTF-8, fallback to original if conversion fails */
        char *safe_name = g_filename_to_utf8(name, -1, NULL, NULL, NULL);
        if (!safe_name)
            safe_name = g_strdup(name);

        /* Build full path */
        char *path = g_build_filename(folder, name, NULL);

        /* Convert path safely to UTF-8 */
        char *safe_path = g_filename_to_utf8(path, -1, NULL, NULL, NULL);
        if (!safe_path)
            safe_path = g_strdup(path);

        /* Skip files/folders that don’t exist yet (prevents invalid_name glitches) */
        if (!g_file_test(safe_path, G_FILE_TEST_EXISTS)) {
            g_free(safe_path);
            g_free(path);
            g_free(safe_name);
            continue;
        }

        if (g_file_test(safe_path, G_FILE_TEST_IS_DIR))
            dirs = g_list_insert_sorted(dirs, g_strdup(safe_name), (GCompareFunc)g_utf8_collate);
        else
            files = g_list_insert_sorted(files, g_strdup(safe_name), (GCompareFunc)g_utf8_collate);

        g_free(safe_path);
        g_free(path);
        g_free(safe_name);
    }

    g_dir_close(dir);

    /* Append directories first */
    for (GList *l = dirs; l; l = l->next) {
        char *dname = l->data;
        char *dpath = g_build_filename(folder, dname, NULL);

        GtkTreeIter child;
        gtk_tree_store_append(app->tree_store, &child, parent);
        gtk_tree_store_set(app->tree_store, &child,
                           0, g_strdup(dname),
                           1, g_strdup(dpath),
                           -1);

        tree_add_children(app, &child, dpath);

        g_free(dpath);
        g_free(dname);
    }

    /* Append files */
    for (GList *l = files; l; l = l->next) {
        char *fname = l->data;
        char *fpath = g_build_filename(folder, fname, NULL);

        GtkTreeIter child;
        gtk_tree_store_append(app->tree_store, &child, parent);
        gtk_tree_store_set(app->tree_store, &child,
                           0, g_strdup(fname),
                           1, g_strdup(fpath),
                           -1);

        g_free(fpath);
        g_free(fname);
    }

    g_list_free(dirs);
    g_list_free(files);
}

/* populate the entire tree view */
static void tree_populate(AppState *app, const char *folder) {
    if (!folder || !g_file_test(folder, G_FILE_TEST_IS_DIR)) {
        app_log(app, "▸ Tree: folder not found.");
        return;
    }

    /* update stored folder before clearing */
    char *saved = g_strdup(folder);
    g_free(app->tree_folder);
    app->tree_folder = saved;

    gtk_tree_store_clear(app->tree_store);

    /* UTF-8 safe root name */
    char *root_name = g_path_get_basename(folder);
    char *safe_root_name = g_filename_to_utf8(root_name, -1, NULL, NULL, NULL);
    if (!safe_root_name) safe_root_name = g_strdup(root_name);

    char *root_label = g_strdup_printf("📁 %s", safe_root_name);

    GtkTreeIter root;
    gtk_tree_store_append(app->tree_store, &root, NULL);
    gtk_tree_store_set(app->tree_store, &root,
                       0, root_label,
                       1, folder,
                       -1);

    tree_add_children(app, &root, folder);

    g_free(root_name);
    g_free(safe_root_name);
    g_free(root_label);

    /* expand root row */
    GtkTreePath *tp = gtk_tree_model_get_path(GTK_TREE_MODEL(app->tree_store), &root);
    gtk_tree_view_expand_row(GTK_TREE_VIEW(app->tree_view), tp, FALSE);
    gtk_tree_path_free(tp);

}

static void on_tree_row_activated(GtkTreeView *tv, GtkTreePath *path,
                                   GtkTreeViewColumn *col, AppState *app) {
    GtkTreeIter it;
    gtk_tree_model_get_iter(GTK_TREE_MODEL(app->tree_store), &it, path);
    char *fpath = NULL;
    gtk_tree_model_get(GTK_TREE_MODEL(app->tree_store), &it, 1, &fpath, -1);
    if (fpath && g_file_test(fpath, G_FILE_TEST_IS_REGULAR))
        tab_new(app, fpath);
    g_free(fpath);
}

static void on_tree_pick_folder(GtkButton *btn, AppState *app) {
    GtkWidget *dlg = gtk_file_chooser_dialog_new(
        "Choose Folder", GTK_WINDOW(app->window),
        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open",   GTK_RESPONSE_ACCEPT,
        NULL);
    if (app->tree_folder)
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg),
            app->tree_folder);
    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
        char *folder = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        tree_populate(app, folder);
        g_free(folder);
    }
    gtk_widget_destroy(dlg);
}

static void on_tree_refresh(GtkButton *btn, AppState *app) {
    if (!app->tree_folder || !app->tree_folder[0]) {
        app_log(app, "▸ No folder set.");
        return;
    }
    char *folder = g_strdup(app->tree_folder);
    tree_populate(app, folder);
    g_free(folder);
    app_log(app, "▸ Tree refreshed.");
}

static void on_tree_toggle(GtkMenuItem *item, AppState *app) {
    if (gtk_widget_get_visible(app->tree_panel)) {
        gtk_widget_hide(app->tree_panel);
        gtk_menu_item_set_label(
            GTK_MENU_ITEM(app->tree_toggle_item_widget), "SHOW TREE");
    } else {
        gtk_widget_show(app->tree_panel);
        gtk_menu_item_set_label(
            GTK_MENU_ITEM(app->tree_toggle_item_widget), "HIDE TREE");
    }
}

static void build_ui(AppState *app) {
    /* ── window ───────────────────────────────────────────────────── */
    app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app->window), APP_TITLE);
    gtk_window_set_default_size(GTK_WINDOW(app->window), 900, 660);
    gtk_container_set_border_width(GTK_CONTAINER(app->window), 10);
    g_signal_connect(app->window, "destroy", G_CALLBACK(on_destroy), app);

    /* ── CSS ──────────────────────────────────────────────────────── */
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, APP_CSS, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    app->size_provider = gtk_css_provider_new();
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(app->size_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);

    /* ── outer box ────────────────────────────────────────────────── */
    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_add(GTK_CONTAINER(app->window), outer);

    /* title */
    GtkWidget *title = gtk_label_new("▸ CYON DEV NOTES // TTS");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "title-label");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(outer), title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(outer),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 2);

    /* ── file toolbar ─────────────────────────────────────────────── */
    GtkWidget *file_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(outer), file_bar, FALSE, FALSE, 0);

    app->filename_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->filename_entry),
        "filename  (no extension — .txt auto-added)");
    gtk_box_pack_start(GTK_BOX(file_bar), app->filename_entry, TRUE, TRUE, 0);

    /* FILE menu button */
    GtkWidget *menu_btn = gtk_button_new_with_label("FILE");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(menu_btn), "btn-menu");
    gtk_box_pack_start(GTK_BOX(file_bar), menu_btn, FALSE, FALSE, 0);

    /* FILE menu */
    app->file_menu = gtk_menu_new();
    struct { const char *label; GCallback cb; } menu_items[] = {
        { "NEW TAB",       G_CALLBACK(on_new_tab)      },
        { "NEW",           G_CALLBACK(on_new)           },
        { "LOAD",          G_CALLBACK(on_load)          },
        { "SAVE",          G_CALLBACK(on_save)          },
        { "SAVE AS",       G_CALLBACK(on_save_as)       },
        { NULL,            NULL                         },
        { "UNDO  Ctrl+Z",  G_CALLBACK(on_undo)          },
        { "REDO  Ctrl+Y",  G_CALLBACK(on_redo)          },
        { NULL,            NULL                         },
        { "DELETE FILE",   G_CALLBACK(on_delete_file)   },
        { NULL,            NULL                         },
        { "TEXT  +",       G_CALLBACK(on_text_size_inc) },
        { "TEXT  −",       G_CALLBACK(on_text_size_dec) },
        { NULL,            NULL                         },
        { "HIDE TREE",     G_CALLBACK(on_tree_toggle)   },
    };
    for (size_t i = 0; i < G_N_ELEMENTS(menu_items); i++) {
        if (!menu_items[i].label) {
            gtk_menu_shell_append(GTK_MENU_SHELL(app->file_menu),
                gtk_separator_menu_item_new());
        } else {
            GtkWidget *it = make_menu_item(
                menu_items[i].label, menu_items[i].cb, app);
            gtk_menu_shell_append(GTK_MENU_SHELL(app->file_menu), it);
            if (strcmp(menu_items[i].label, "HIDE TREE") == 0)
                app->tree_toggle_item_widget = it;
        }
    }
    gtk_widget_show_all(app->file_menu);
    g_signal_connect(menu_btn, "clicked",
        G_CALLBACK(on_file_menu_clicked), app);

    /* ── TTS toolbar ──────────────────────────────────────────────── */
    GtkWidget *tts_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(outer), tts_bar, FALSE, FALSE, 0);

    GtkWidget *tts_lbl = gtk_label_new("TTS:");
    gtk_style_context_add_class(gtk_widget_get_style_context(tts_lbl), "section-label");
    gtk_box_pack_start(GTK_BOX(tts_bar), tts_lbl, FALSE, FALSE, 0);

    struct { const char *lbl; GCallback cb; } tts_btns[] = {
        { "▶ SELECTION", G_CALLBACK(on_tts_speak_selection) },
        { "▶ ALL",       G_CALLBACK(on_tts_speak_all)       },
        { "■ STOP",      G_CALLBACK(on_tts_stop)            },
        { "JOE",         G_CALLBACK(on_tts_voice_joe)       },
        { "LESSAC",      G_CALLBACK(on_tts_voice_lessac)    },
    };
    for (size_t i = 0; i < G_N_ELEMENTS(tts_btns); i++) {
        GtkWidget *b = gtk_button_new_with_label(tts_btns[i].lbl);
        gtk_style_context_add_class(gtk_widget_get_style_context(b), "btn-menu");
        g_signal_connect(b, "clicked", tts_btns[i].cb, app);
        gtk_box_pack_start(GTK_BOX(tts_bar), b, FALSE, FALSE, 0);
    }

    /* cursor pos label on right of tts bar */
    app->cursor_label = gtk_label_new("Ln 1  Col 1");
    gtk_style_context_add_class(gtk_widget_get_style_context(app->cursor_label), "section-label");
    gtk_widget_set_halign(app->cursor_label, GTK_ALIGN_END);
    gtk_box_pack_end(GTK_BOX(tts_bar), app->cursor_label, FALSE, FALSE, 8);

    /* ── notebook ─────────────────────────────────────────────────── */
    app->notebook = gtk_notebook_new();
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(app->notebook), TRUE);
    gtk_notebook_set_show_border(GTK_NOTEBOOK(app->notebook), FALSE);
    g_signal_connect(app->notebook, "switch-page",
        G_CALLBACK(on_tab_switched), app);

    /* ── tree panel ──────────────────────────────────────────────── */
    app->tree_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(app->tree_panel), "tree-panel");

    /* header row */
    GtkWidget *tree_hdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_container_set_border_width(GTK_CONTAINER(tree_hdr), 4);

    GtkWidget *tree_lbl = gtk_label_new("▸ FILES");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(tree_lbl), "tree-header");
    gtk_widget_set_halign(tree_lbl, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(tree_hdr), tree_lbl, TRUE, TRUE, 0);

    GtkWidget *btn_pick = gtk_button_new_with_label("⊞ FOLDER");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(btn_pick), "btn-tree");
    g_signal_connect(btn_pick, "clicked",
        G_CALLBACK(on_tree_pick_folder), app);
    gtk_box_pack_start(GTK_BOX(tree_hdr), btn_pick, FALSE, FALSE, 0);

    GtkWidget *btn_refresh = gtk_button_new_with_label("⟳");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(btn_refresh), "btn-tree");
    g_signal_connect(btn_refresh, "clicked",
        G_CALLBACK(on_tree_refresh), app);
    gtk_box_pack_start(GTK_BOX(tree_hdr), btn_refresh, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(app->tree_panel), tree_hdr, FALSE, FALSE, 0);

    /* tree view */
    app->tree_store = gtk_tree_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    app->tree_view  = gtk_tree_view_new_with_model(
        GTK_TREE_MODEL(app->tree_store));
    gtk_widget_set_name(app->tree_view, "tree-view");
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(app->tree_view), FALSE);
    gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(app->tree_view), TRUE);
    GtkCellRenderer   *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *col = gtk_tree_view_column_new_with_attributes(
        "File", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(app->tree_view), col);
    g_signal_connect(app->tree_view, "row-activated",
        G_CALLBACK(on_tree_row_activated), app);

    GtkWidget *tree_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tree_scroll),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(tree_scroll), app->tree_view);
    gtk_box_pack_start(GTK_BOX(app->tree_panel), tree_scroll, TRUE, TRUE, 0);

    /* ── paned: notebook left, tree right ────────────────────────── */
    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_pack1(GTK_PANED(paned), app->notebook,   TRUE,  TRUE);
    gtk_paned_pack2(GTK_PANED(paned), app->tree_panel, FALSE, FALSE);
    gtk_paned_set_position(GTK_PANED(paned), 600);
    gtk_box_pack_start(GTK_BOX(outer), paned, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(outer),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 2);

    /* ── log terminal ─────────────────────────────────────────────── */
    GtkWidget *log_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_size_request(log_scroll, -1, 72);
    app->log_view = gtk_text_view_new();
    gtk_widget_set_name(app->log_view, "log-view");
    gtk_text_view_set_editable(GTK_TEXT_VIEW(app->log_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(app->log_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(app->log_view), GTK_WRAP_WORD_CHAR);
    app->log_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->log_view));
    gtk_container_add(GTK_CONTAINER(log_scroll), app->log_view);
    gtk_box_pack_start(GTK_BOX(outer), log_scroll, FALSE, FALSE, 0);

    /* ── status bar ───────────────────────────────────────────────── */
    char *notes_dir = get_notes_dir();
    char *status_text = g_strdup_printf("dir: %s", notes_dir);
    app->status_label = gtk_label_new(status_text);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(app->status_label), "status-bar");
    gtk_widget_set_halign(app->status_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(outer), app->status_label, FALSE, FALSE, 0);
    g_free(status_text);
    g_free(notes_dir);

    gtk_widget_show_all(app->window);

    /* load syntax highlighting config */
    hl_load_config(app);

    /* ── open initial blank tab ───────────────────────────────────── */
    tab_new(app, NULL);
    /* populate tree with notes dir */
    app->tree_folder = NULL;
    app->tree_monitor = NULL;
    app->tree_refresh_timer = 0;
    char *nd = get_notes_dir();
    tree_populate(app, nd);
    g_free(nd);

    app_log(app, "▸ Ready.");
}

/* ── main ───────────────────────────────────────────────────────────────── */

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);

    if (!acquire_lock()) {
        GtkWidget *dlg = gtk_message_dialog_new(
            NULL, 0, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
            "cyon_notes is already running.");
        gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_widget_destroy(dlg);
        return 1;
    }

    ensure_notes_dir();

    AppState *app = g_new0(AppState, 1);
    app->font_size     = DEFAULT_FONT_PX;
    app->tts_pid       = 0;
    app->tts_voice_joe = TRUE;

    build_ui(app);

    gtk_main();

    g_free(app);
    return 0;
}
