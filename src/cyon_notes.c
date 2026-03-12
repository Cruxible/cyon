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
#define NOTES_DIR_NAME  "/cyon"
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
"  font-size: 15px;"
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
"  font-size: 16px;"
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
"  font-size: 20px;"
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
    "  padding: 7px 12px;"
    "  min-width: 0;"
    "}"
    ".btn-tree:hover { color: #00ff99; border-color: #E8A020; }"

    "#tree-view {"
    "  background-color: #080810;"
    "  color: #00cc77;"
    "  font-family: monospace;"
    "  font-size: 14px;"
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

    /* debounce: defer expensive work after rapid input (e.g. large paste) */
    guint          hl_timer;      /* pending highlight timeout id, 0=none */
    guint          undo_timer;    /* pending undo snapshot timeout id, 0=none */
    int            last_line_count; /* for cheap line-number change detection */

    /* dirty-line tracking for incremental highlight */
    GArray        *dirty_lines;   /* sorted array of gint line numbers */
    gboolean       hl_full_pending; /* full rehighlight needed (e.g. load) */

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
    gboolean       hl_enabled;   /* syntax highlighting on/off */
    GtkWidget     *hl_btn;       /* the toolbar toggle button */

    /* TTS */
    GPid           tts_pid;         /* currently running piper pid, 0=none */
    gboolean       tts_voice_joe;   /* TRUE=joe (male), FALSE=lessac (female) */
    GtkWidget     *tts_menu;        /* popup */
    char          *tts_piper_path;
    char          *tts_model_dir;
    char          *tts_model_joe;
    char          *tts_model_lessac;

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

    /* RAM graph */
    GtkWidget     *ram_graph;       /* GtkDrawingArea */
    float          ram_samples[120];/* rolling RSS samples in MB */
    int            ram_head;        /* next write index */
    int            ram_count;       /* samples filled so far */
    guint          ram_timer;       /* g_timeout id */
    /* graph colors — loaded from [ram_graph] in config */
    double         ram_col_line[3]; /* main line RGB */
    double         ram_col_glow[3]; /* glow RGB */
    double         ram_col_grid[3]; /* grid RGB */
    double         ram_col_text[3]; /* label RGB */

    /* log colors */
    GtkTextTag    *log_tag_amber;

    /* layout — saved/restored */
    GtkWidget     *paned;           /* main h-paned for tree position */

    /* startup config scratch — used between app_load_config and build_ui */
    gboolean       _cfg_tree_visible;
    gint           _cfg_pane_pos;
    gint           _cfg_win_w;
    gint           _cfg_win_h;
    gint           last_win_w;   /* updated live via configure-event */
    gint           last_win_h;
    char          *_cfg_notes_dir;
    gchar        **_cfg_open_files;
    int            _cfg_open_files_n;
    int            _cfg_active_tab;
};


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
static void tab_mark_visible_dirty(EditorTab *tab);
static gboolean hl_deferred_cb(EditorTab *tab);
static void parse_hex_color(const char *hex, double out[3], const double def[3]);
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

/* ── dirty-line helpers ─────────────────────────────────────────────────── */

/* Mark a single line dirty for incremental rehighlight */
static void tab_mark_line_dirty(EditorTab *tab, gint line) {
    if (!tab->app->hl_enabled) return;
    /* insert sorted, skip duplicates */
    GArray *a = tab->dirty_lines;
    for (guint i = 0; i < a->len; i++) {
        gint v = g_array_index(a, gint, i);
        if (v == line) return;
        if (v > line) { g_array_insert_val(a, i, line); return; }
    }
    g_array_append_val(a, line);
}

/* Mark the entire visible region dirty (used after scroll or load) */
static void tab_mark_visible_dirty(EditorTab *tab) {
    if (!tab->app->hl_enabled || !tab->text_view) return;
    GdkRectangle vis;
    gtk_text_view_get_visible_rect(GTK_TEXT_VIEW(tab->text_view), &vis);
    GtkTextIter s, e;
    gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(tab->text_view), &s, vis.x, vis.y);
    gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(tab->text_view), &e, vis.x, vis.y + vis.height);
    gint sl = gtk_text_iter_get_line(&s);
    gint el = gtk_text_iter_get_line(&e);
    for (gint l = sl; l <= el; l++) tab_mark_line_dirty(tab, l);
}

/* ── insert/delete signal handlers to capture dirty lines ───────────────── */

static void on_buf_insert(GtkTextBuffer *buf, GtkTextIter *pos,
                          gchar *text, gint len, EditorTab *tab) {
    gint line = gtk_text_iter_get_line(pos);
    tab_mark_line_dirty(tab, line);
    /* if inserting newlines, mark following lines dirty too */
    if (len > 0 && memchr(text, '\n', len)) {
        GtkTextIter end = *pos;
        gtk_text_iter_forward_chars(&end, g_utf8_strlen(text, len));
        gint end_line = gtk_text_iter_get_line(&end);
        for (gint l = line + 1; l <= end_line; l++) tab_mark_line_dirty(tab, l);
    }
}

static void on_buf_delete(GtkTextBuffer *buf, GtkTextIter *start,
                          GtkTextIter *end, EditorTab *tab) {
    gint sl = gtk_text_iter_get_line(start);
    gint el = gtk_text_iter_get_line(end);
    for (gint l = sl; l <= el; l++) tab_mark_line_dirty(tab, l);
}

/* ── incremental highlight: only process dirty lines ────────────────────── */

static void hl_apply_lines(AppState *app, EditorTab *tab);

/* ── click scroll guard ─────────────────────────────────────────────────────
 * GTK's GtkTextView calls gtk_text_view_scroll_mark_onscreen on every click,
 * jumping the view to wherever the cursor lands even if the view is already
 * showing that area. We intercept button-press, save scroll, and restore it
 * after GTK finishes its internal scroll. */

typedef struct { GtkAdjustment *adj; gdouble val; } ClickScrollGuard;

static gboolean click_scroll_restore_cb(gpointer data) {
    ClickScrollGuard *g = data;
    gtk_adjustment_set_value(g->adj, g->val);
    g_free(g);
    return G_SOURCE_REMOVE;
}

/* ── glitch popup ───────────────────────────────────────────────────────────
 * A small undecorated window that appears near the click spot, holds for
 * 900ms, then destroys itself. Owns the scroll-restore flicker. */

static gboolean glitch_popup_close_cb(gpointer data) {
    GtkWidget *win = GTK_WIDGET(data);
    gtk_widget_destroy(win);
    return G_SOURCE_REMOVE;
}

static void show_glitch_popup(AppState *app, gint root_x, gint root_y) {
    /* Only fire occasionally — 1 in 4 clicks, keeps it from getting annoying */
    if (g_random_int_range(0, 4) != 0) return;

    GtkWidget *popup = gtk_window_new(GTK_WINDOW_POPUP);
    gtk_window_set_transient_for(GTK_WINDOW(popup), GTK_WINDOW(app->window));
    gtk_window_set_resizable(GTK_WINDOW(popup), FALSE);
    gtk_window_set_decorated(GTK_WINDOW(popup), FALSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(popup), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(popup), TRUE);
    gtk_window_set_keep_above(GTK_WINDOW(popup), TRUE);

    /* Style scoped to popup widget only — NOT screen-wide */
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
        "* { background-color: #05050a; border: 1px solid #E8A020; }"
        "label { color: #00ff99; font-family: monospace; font-size: 12px;"
        "        padding: 8px 14px; }",
        -1, NULL);
    gtk_style_context_add_provider(
        gtk_widget_get_style_context(popup),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);

    GtkWidget *lbl = gtk_label_new(
        "> GLITCH EVENT DETECTED\n"
        "  this was a bug.\n"
        "  now it's a feature.\n"
        "  -- cruxibulum");
    gtk_label_set_justify(GTK_LABEL(lbl), GTK_JUSTIFY_LEFT);
    gtk_container_add(GTK_CONTAINER(popup), lbl);

    /* Position near click, nudge so it doesn't sit under the cursor */
    gtk_window_move(GTK_WINDOW(popup), root_x + 12, root_y + 12);
    gtk_widget_show_all(popup);

    /* Auto-close after 900ms */
    g_timeout_add(900, glitch_popup_close_cb, popup);
}

static gboolean on_textview_button_press(GtkWidget *widget,
                                         GdkEventButton *event,
                                         gpointer data) {
    EditorTab *tab = (EditorTab *)data;
    GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(
        GTK_SCROLLED_WINDOW(tab->scroll));
    gdouble current = gtk_adjustment_get_value(vadj);

    /* Schedule restore AFTER GTK processes the click and moves the mark */
    ClickScrollGuard *g1 = g_new(ClickScrollGuard, 1);
    g1->adj = vadj; g1->val = current;
    g_idle_add(click_scroll_restore_cb, g1);

    ClickScrollGuard *g2 = g_new(ClickScrollGuard, 1);
    g2->adj = vadj; g2->val = current;
    g_timeout_add(50, click_scroll_restore_cb, g2);

    /* glitch popup — near the click, auto-closes, occasional */
    if (event->button == 1)
        show_glitch_popup(tab->app,
                          (gint)event->x_root,
                          (gint)event->y_root);

    return FALSE;  /* don't block the event — let GTK place the cursor normally */
}

/* forward — ScrollRestore is defined after this function in the file */
static gboolean hl_scroll_restore_cb(gpointer data) {
    GtkAdjustment *adj = ((GtkAdjustment **)data)[0];
    gdouble        val = *((gdouble *)(((GtkAdjustment **)data) + 1));
    gtk_adjustment_set_value(adj, val);
    g_free(data);
    return G_SOURCE_REMOVE;
}

static gpointer make_scroll_data(GtkAdjustment *adj, gdouble val) {
    /* Pack adj pointer + double into a single g_malloc'd block */
    guchar *block = g_malloc(sizeof(GtkAdjustment *) + sizeof(gdouble));
    *(GtkAdjustment **)block = adj;
    *(gdouble *)(block + sizeof(GtkAdjustment *)) = val;
    return block;
}

static gboolean hl_deferred_cb(EditorTab *tab) {
    tab->hl_timer = 0;
    if (tab->hl_full_pending) {
        tab->hl_full_pending = FALSE;
        /* Save scroll before full highlight — tag signals can trigger
         * scroll-to-cursor internally and jump view to top */
        GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(
            GTK_SCROLLED_WINDOW(tab->scroll));
        gdouble saved = gtk_adjustment_get_value(vadj);
        hl_apply(tab->app, tab->text_buf);
        /* Restore twice: idle catches the first reset, timeout catches
         * any secondary reset from focus or tag layout events */
        g_idle_add(hl_scroll_restore_cb, make_scroll_data(vadj, saved));
        g_timeout_add(80, hl_scroll_restore_cb, make_scroll_data(vadj, saved));
    } else {
        hl_apply_lines(tab->app, tab);
    }
    return G_SOURCE_REMOVE;
}

/* ── debounced undo snapshot callback ──────────────────────────────────── */

static gboolean undo_deferred_cb(EditorTab *tab) {
    tab->undo_timer = 0;
    GtkTextIter s, e;
    gtk_text_buffer_get_bounds(tab->text_buf, &s, &e);
    char *snap = gtk_text_buffer_get_text(tab->text_buf, &s, &e, FALSE);
    undo_stack_push(&tab->undo_stack, snap);
    undo_stack_clear(&tab->redo_stack);
    g_free(snap);
    return G_SOURCE_REMOVE;
}

static void on_text_changed(GtkTextBuffer *buf, EditorTab *tab) {
    if (!tab->undo_inhibit) {
        tab->is_dirty = TRUE;
        if (!tab->undo_timer)
            tab->undo_timer = g_timeout_add(500, (GSourceFunc)undo_deferred_cb, tab);
    }

    /* dirty lines are marked by on_buf_insert/on_buf_delete before this fires;
     * just schedule the deferred pass if not already pending. */
    if (tab->app->hl_enabled && !tab->hl_timer)
        tab->hl_timer = g_timeout_add(80, (GSourceFunc)hl_deferred_cb, tab);

    /* Line numbers: only rebuild when line count changes. */
    int new_count = gtk_text_buffer_get_line_count(buf);
    if (new_count != tab->last_line_count) {
        tab->last_line_count = new_count;
        tab_update_line_numbers(tab);
    }
}

/* ── scroll restore helper ──────────────────────────────────────────────── */

typedef struct { GtkAdjustment *adj; gdouble val; } ScrollRestore;

static gboolean restore_scroll_cb(ScrollRestore *sr) {
    gtk_adjustment_set_value(sr->adj, sr->val);
    g_free(sr);
    return G_SOURCE_REMOVE;
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

    /* save scroll position */
    GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(tab->scroll));
    gdouble scroll_pos = gtk_adjustment_get_value(vadj);

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

    /* full highlight pass needed after load */
    tab->hl_full_pending = TRUE;
    if (tab->app->hl_enabled && !tab->hl_timer)
        tab->hl_timer = g_timeout_add(80, (GSourceFunc)hl_deferred_cb, tab);
    tab_update_line_numbers(tab);
    tab_update_label(tab);
    app_log(tab->app, "▸ Loaded: %s", path);

    /* Restore scroll twice — idle fires first, but GTK may reset again on
     * focus-in or tag changes; the 80ms timeout catches that second reset. */
    ScrollRestore *sr1 = g_new(ScrollRestore, 1);
    sr1->adj = vadj; sr1->val = scroll_pos;
    g_idle_add((GSourceFunc)restore_scroll_cb, sr1);
    ScrollRestore *sr2 = g_new(ScrollRestore, 1);
    sr2->adj = vadj; sr2->val = scroll_pos;
    g_timeout_add(80, (GSourceFunc)restore_scroll_cb, sr2);
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
    tab->hl_timer        = 0;
    tab->undo_timer      = 0;
    tab->last_line_count = 0;
    tab->dirty_lines     = g_array_new(FALSE, FALSE, sizeof(gint));
    tab->hl_full_pending = FALSE;
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

    /* dirty-line tracking — must connect before "changed" so line info is valid */
    g_signal_connect(tab->text_buf, "insert-text",
        G_CALLBACK(on_buf_insert), tab);
    g_signal_connect(tab->text_buf, "delete-range",
        G_CALLBACK(on_buf_delete), tab);

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

    /* Intercept clicks to prevent GTK's scroll-to-cursor from jumping the view */
    g_signal_connect(tab->text_view, "button-press-event",
        G_CALLBACK(on_textview_button_press), tab);

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

    gtk_notebook_set_current_page(GTK_NOTEBOOK(app->notebook), page_idx);
    gtk_widget_show_all(app->notebook);

    gtk_entry_set_text(GTK_ENTRY(app->filename_entry),
        path ? tab_display_name(tab) : "");

    /* Grab focus FIRST so GTK fires its internal scroll-to-cursor on an empty
     * buffer — then tab_load_file loads content and the deferred restore wins. */
    gtk_widget_grab_focus(tab->text_view);

    /* ── load file after focus grab ── */
    if (path) {
        tab_load_file(tab, path);
    }

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
        /* cancel any pending debounce timers before freeing */
        if (tab->hl_timer)   { g_source_remove(tab->hl_timer);   tab->hl_timer   = 0; }
        if (tab->undo_timer) { g_source_remove(tab->undo_timer); tab->undo_timer = 0; }
        /* free the tab */
        undo_stack_free(&tab->undo_stack);
        undo_stack_free(&tab->redo_stack);
        g_free(tab->current_file);
        if (tab->dirty_lines) g_array_free(tab->dirty_lines, TRUE);
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

    /* Scroll to end using a mark — more reliable than scroll_to_iter
     * because marks stay valid after buffer changes and GTK layout */
    GtkTextMark *mark = gtk_text_buffer_get_mark(app->log_buf, "log-end");
    if (!mark) {
        gtk_text_buffer_get_end_iter(app->log_buf, &end);
        mark = gtk_text_buffer_create_mark(app->log_buf, "log-end", &end, FALSE);
    } else {
        gtk_text_buffer_get_end_iter(app->log_buf, &end);
        gtk_text_buffer_move_mark(app->log_buf, mark, &end);
    }
    gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(app->log_view), mark);

    gtk_label_set_text(GTK_LABEL(app->status_label), msg);
    g_free(msg);
}

static void app_log_amber(AppState *app, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char *msg = g_strdup_vprintf(fmt, args);
    va_end(args);

    /* create amber tag once */
    if (!app->log_tag_amber)
        app->log_tag_amber = gtk_text_buffer_create_tag(
            app->log_buf, "log-amber",
            "foreground", "#E8A020", NULL);

    GtkTextIter start, end;
    gtk_text_buffer_get_end_iter(app->log_buf, &start);
    gtk_text_buffer_insert(app->log_buf, &start, msg, -1);
    gtk_text_buffer_get_end_iter(app->log_buf, &end);
    gtk_text_buffer_insert(app->log_buf, &end, "\n", -1);

    /* re-get start after insert invalidated it */
    gint line = gtk_text_iter_get_line(&end) - 1;
    GtkTextIter ls, le;
    gtk_text_buffer_get_iter_at_line(app->log_buf, &ls, line);
    le = ls;
    gtk_text_iter_forward_to_line_end(&le);
    gtk_text_buffer_apply_tag(app->log_buf, app->log_tag_amber, &ls, &le);

    GtkTextMark *mark = gtk_text_buffer_get_mark(app->log_buf, "log-end");
    if (!mark) {
        gtk_text_buffer_get_end_iter(app->log_buf, &end);
        mark = gtk_text_buffer_create_mark(app->log_buf, "log-end", &end, FALSE);
    } else {
        gtk_text_buffer_get_end_iter(app->log_buf, &end);
        gtk_text_buffer_move_mark(app->log_buf, mark, &end);
    }
    gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(app->log_view), mark);

    gtk_label_set_text(GTK_LABEL(app->status_label), msg);
    g_free(msg);
}



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
    /* Ctrl+A — select all */
    if (ctrl && ev->keyval == GDK_KEY_a) {
        GtkTextIter s, e;
        gtk_text_buffer_get_bounds(buf, &s, &e);
        gtk_text_buffer_select_range(buf, &s, &e);
        return TRUE;
    }
    /* Ctrl+W — close current tab */
    if (ctrl && ev->keyval == GDK_KEY_w) {
        GtkWidget *page = gtk_notebook_get_nth_page(
            GTK_NOTEBOOK(app->notebook),
            gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook)));
        if (page) on_tab_close_clicked(NULL, page);
        return TRUE;
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
            /* strip trailing ( and : from word so "else:" matches "else" */
            char *paren = strchr(first_word, '(');
            if (paren) *paren = '\0';
            char *colon = strchr(first_word, ':');
            if (colon) *colon = '\0';
            for (int i = 0; triggers[i]; i++) {
                if (strcmp(first_word, triggers[i]) == 0) {
                    add_indent = TRUE; break;
                }
            }
        }
        /* also indent after an open bracket or brace at end of line */
        if (!add_indent && slen > 0) {
            const char *end = stripped + slen - 1;
            while (end > stripped && *end == ' ') end--;
            if (*end == '[' || *end == '{') add_indent = TRUE;
        }
        g_free(line_text);

        int total = base_indent + (add_indent ? 4 : 0);

        /* build "\n" + spaces */
        GString *ins = g_string_new("\n");
        for (int i = 0; i < total; i++) g_string_append_c(ins, ' ');
        gtk_text_buffer_insert_at_cursor(buf, ins->str, -1);
        g_string_free(ins, TRUE);
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
    "keywords = def class import from as lambda global nonlocal del\n"
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
    "keywords = len range open map filter list dict set tuple int float str bool type isinstance enumerate zip sorted reversed sum min max abs round hasattr getattr setattr callable\n"
    "color = #00ffaa\n"
    "\n"
    "[highlight_group_7]\n"
    "keywords = gboolean gint gchar GtkWidget GtkWindow GtkBox GtkButton GtkLabel\n"
    "color = #8fd3ff\n"
    "\n"
    "[highlight_group_8]\n"
    "keywords = expanduser join append extend insert pop update get items keys values function source export alias unset\n"
    "color = #5fd7ff\n"
    "\n"
    "[highlight_group_9]\n"
    "keywords = and or not in is\n"
    "color = #ffd700\n"
    "\n"
    "[highlight_group_10]\n"
    "keywords = os sys subprocess pathlib read write mkdir remove rename chmod chown stat glob shutil copy move cd ls cat grep sed awk find ps kill sudo apt git\n"
    "color = #00eaff\n"
    "\n"
    "[highlight_group_11]\n"
    "keywords = eval exec compile __import__ malloc free calloc realloc goto\n"
    "color = #ff5555\n"
    "\n"
    "[highlight_group_12]\n"
    "keywords = breakpoint help dir vars globals locals __name__ __file__ __doc__ __all__ __init__ __main__\n"
    "color = #c792ea\n"
    "\n"
    "[highlight_group_13]\n"
    "keywords = void static const char unsigned signed long short struct union enum typedef extern inline auto register volatile restrict\n"
    "color = #8fd3ff\n"
    "\n"
    "[highlight_group_14]\n"
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
    /* Create a named-NULL tag on the given buffer with the specified foreground
     * color. Tags are stored on AppState and applied to any tab buffer via the
     * shared tag table (g_shared_tags). */
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

/* Apply a tag to all matches of a GRegex in a text slice.
 * base_offset: character offset in the buffer where `text` begins. */
static void hl_apply_regex(GtkTextBuffer *buf, GRegex *re,
                            GtkTextTag *tag, const char *text,
                            int group, gint base_offset) {
    if (!re || !tag) return;
    GMatchInfo *mi = NULL;
    g_regex_match(re, text, 0, &mi);
    while (g_match_info_matches(mi)) {
        gint s, e;
        g_match_info_fetch_pos(mi, group, &s, &e);
        if (s >= 0 && e > s) {
            glong cs = base_offset + g_utf8_pointer_to_offset(text, text + s);
            glong ce = base_offset + g_utf8_pointer_to_offset(text, text + e);
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
    if (!app->hl_enabled) return;      /* highlighting toggled off */

    /* ── Only highlight the visible region + a small margin.
     *    Scanning the whole buffer is O(n) in doc size; the visible
     *    window is effectively O(1) regardless of file length.       ── */
    EditorTab *tab = NULL;
    {
        int n = gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook));
        for (int i = 0; i < n && !tab; i++) {
            GtkWidget *pg = gtk_notebook_get_nth_page(GTK_NOTEBOOK(app->notebook), i);
            EditorTab *t  = g_object_get_data(G_OBJECT(pg), "editor-tab");
            if (t && t->text_buf == buf) tab = t;
        }
    }

    GtkTextIter s, e;
    if (tab && tab->text_view) {
        GdkRectangle vis;
        gtk_text_view_get_visible_rect(GTK_TEXT_VIEW(tab->text_view), &vis);
        /* 10-line margin so scrolling doesn't show unhighlighted text */
        int margin = MAX(vis.height / 3, 200);
        int top_y    = MAX(0, vis.y - margin);
        int bottom_y = vis.y + vis.height + margin;
        gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(tab->text_view), &s, vis.x, top_y);
        gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(tab->text_view), &e, vis.x, bottom_y);
        gtk_text_iter_set_line_offset(&s, 0);
        if (!gtk_text_iter_ends_line(&e))
            gtk_text_iter_forward_to_line_end(&e);
    } else {
        gtk_text_buffer_get_bounds(buf, &s, &e);
    }

    gint base_offset = gtk_text_iter_get_offset(&s);

    /* clear tags only in the region we are about to repaint */
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

    /* 1) strings — high contrast, paint first */
    {
        static GRegex *re_str = NULL;
        if (!re_str) re_str = g_regex_new("(['\"])((?:\\\\.|[^\\\\'\"\\n])*)\\1",
                                          G_REGEX_OPTIMIZE, 0, NULL);
        GMatchInfo *mi = NULL;
        g_regex_match(re_str, text, 0, &mi);
        while (g_match_info_matches(mi)) {
            gint cs, ce;
            g_match_info_fetch_pos(mi, 2, &cs, &ce);  // only the inner string
            if (cs >= 0 && ce > cs) {
                GtkTextIter si, ei;
                glong ccs = base_offset + g_utf8_pointer_to_offset(text, text + cs);
                glong cce = base_offset + g_utf8_pointer_to_offset(text, text + ce);
                gtk_text_buffer_get_iter_at_offset(buf, &si, (gint)ccs);
                gtk_text_buffer_get_iter_at_offset(buf, &ei, (gint)cce);
                gtk_text_buffer_apply_tag(buf, app->hl_tag_coral, &si, &ei);
            }
            g_match_info_next(mi, NULL);
        }
        g_match_info_free(mi);
    }

    /* 2) keyword groups — lowest priority */
    for (int i = 0; i < app->hl_group_count; i++)
        hl_apply_regex(buf, app->hl_kw_regex[i], app->hl_kw_tags[i], text, 1, base_offset);

    /* 3) steel: standalone = operator */
    {
        static GRegex *re_eq = NULL;
        if (!re_eq) re_eq = g_regex_new("(?<![=!<>])=(?!=)", G_REGEX_OPTIMIZE, 0, NULL);
        hl_apply_regex(buf, re_eq, app->hl_tag_steel, text, 0, base_offset);
    }

    /* 4) steel: @staticmethod */
    {
        static GRegex *re_sm = NULL;
        if (!re_sm) re_sm = g_regex_new("@staticmethod(?!\\w)", G_REGEX_OPTIMIZE, 0, NULL);
        hl_apply_regex(buf, re_sm, app->hl_tag_steel, text, 0, base_offset);
    }

    /* 5) lime: class name after 'class' */
    {
        static GRegex *re_cls = NULL;
        if (!re_cls) re_cls = g_regex_new("(?<!\\w)class\\s+(\\w+)", G_REGEX_OPTIMIZE, 0, NULL);
        hl_apply_regex(buf, re_cls, app->hl_tag_lime, text, 1, base_offset);
    }

    /* 6) dot notation: obj.method — strict word boundaries */
    {
        static GRegex *re_dot = NULL;
        if (!re_dot) re_dot = g_regex_new("\\b(\\w+)\\.(\\w+)\\b", G_REGEX_OPTIMIZE, 0, NULL);
        GMatchInfo *mi = NULL;
        g_regex_match(re_dot, text, 0, &mi);
        while (g_match_info_matches(mi)) {
            gint ls, le, rs, re2;
            g_match_info_fetch_pos(mi, 1, &ls, &le);
            g_match_info_fetch_pos(mi, 2, &rs, &re2);

            if (ls >= 0 && le > ls) {
                GtkTextIter a, b;
                glong cls = base_offset + g_utf8_pointer_to_offset(text, text + ls);
                glong cle = base_offset + g_utf8_pointer_to_offset(text, text + le);
                gtk_text_buffer_get_iter_at_offset(buf, &a, (gint)cls);
                gtk_text_buffer_get_iter_at_offset(buf, &b, (gint)cle);
                gtk_text_buffer_apply_tag(buf, app->hl_tag_dot_l, &a, &b);
            }
            if (rs >= 0 && re2 > rs) {
                GtkTextIter a, b;
                glong crs = base_offset + g_utf8_pointer_to_offset(text, text + rs);
                glong cre = base_offset + g_utf8_pointer_to_offset(text, text + re2);
                gtk_text_buffer_get_iter_at_offset(buf, &a, (gint)crs);
                gtk_text_buffer_get_iter_at_offset(buf, &b, (gint)cre);
                gtk_text_buffer_apply_tag(buf, app->hl_tag_dot_r, &a, &b);
            }

            g_match_info_next(mi, NULL);
        }
        g_match_info_free(mi);
    }

    /* 7) comments — highest priority, paint last */
    {
        static GRegex *re_cmt = NULL;
        if (!re_cmt) re_cmt = g_regex_new("(#[^\n]*|//[^\n]*)", G_REGEX_OPTIMIZE, 0, NULL);
        hl_apply_regex(buf, re_cmt, app->hl_tag_comment, text, 0, base_offset);
    }

    g_free(text);
}

/* ── hl_apply_lines: incremental — only rehighlight dirty lines ─────────── */

static void hl_apply_line(AppState *app, EditorTab *tab, gint line) {
    GtkTextBuffer *buf = tab->text_buf;
    gint total = gtk_text_buffer_get_line_count(buf);
    if (line >= total) return;

    GtkTextIter s, e;
    gtk_text_buffer_get_iter_at_line(buf, &s, line);
    e = s;
    if (!gtk_text_iter_ends_line(&e))
        gtk_text_iter_forward_to_line_end(&e);

    /* clear all tags on this line */
    for (int i = 0; i < app->hl_group_count; i++)
        if (app->hl_kw_tags[i])
            gtk_text_buffer_remove_tag(buf, app->hl_kw_tags[i], &s, &e);
    if (app->hl_tag_lime)    gtk_text_buffer_remove_tag(buf, app->hl_tag_lime,    &s, &e);
    if (app->hl_tag_coral)   gtk_text_buffer_remove_tag(buf, app->hl_tag_coral,   &s, &e);
    if (app->hl_tag_comment) gtk_text_buffer_remove_tag(buf, app->hl_tag_comment, &s, &e);
    if (app->hl_tag_dot_l)   gtk_text_buffer_remove_tag(buf, app->hl_tag_dot_l,   &s, &e);
    if (app->hl_tag_dot_r)   gtk_text_buffer_remove_tag(buf, app->hl_tag_dot_r,   &s, &e);
    if (app->hl_tag_steel)   gtk_text_buffer_remove_tag(buf, app->hl_tag_steel,   &s, &e);

    gint base_offset = gtk_text_iter_get_offset(&s);
    char *text = gtk_text_buffer_get_text(buf, &s, &e, FALSE);
    if (!text || !text[0]) { g_free(text); return; }

    /* strings */
    {
        static GRegex *re_str = NULL;
        if (!re_str) re_str = g_regex_new("(['\"])((?:\\\\.|[^\\\\'\"\\n])*)\\1",
                                          G_REGEX_OPTIMIZE, 0, NULL);
        hl_apply_regex(buf, re_str, app->hl_tag_coral, text, 2, base_offset);
    }
    /* keyword groups */
    for (int i = 0; i < app->hl_group_count; i++)
        hl_apply_regex(buf, app->hl_kw_regex[i], app->hl_kw_tags[i], text, 1, base_offset);
    /* = operator */
    {
        static GRegex *re_eq = NULL;
        if (!re_eq) re_eq = g_regex_new("(?<![=!<>])=(?!=)", G_REGEX_OPTIMIZE, 0, NULL);
        hl_apply_regex(buf, re_eq, app->hl_tag_steel, text, 0, base_offset);
    }
    /* @staticmethod */
    {
        static GRegex *re_sm = NULL;
        if (!re_sm) re_sm = g_regex_new("@staticmethod(?!\\w)", G_REGEX_OPTIMIZE, 0, NULL);
        hl_apply_regex(buf, re_sm, app->hl_tag_steel, text, 0, base_offset);
    }
    /* class name */
    {
        static GRegex *re_cls = NULL;
        if (!re_cls) re_cls = g_regex_new("(?<!\\w)class\\s+(\\w+)", G_REGEX_OPTIMIZE, 0, NULL);
        hl_apply_regex(buf, re_cls, app->hl_tag_lime, text, 1, base_offset);
    }
    /* dot notation */
    {
        static GRegex *re_dot = NULL;
        if (!re_dot) re_dot = g_regex_new("\\b(\\w+)\\.(\\w+)\\b", G_REGEX_OPTIMIZE, 0, NULL);
        GMatchInfo *mi = NULL;
        g_regex_match(re_dot, text, 0, &mi);
        while (g_match_info_matches(mi)) {
            gint ls, le, rs, re2;
            g_match_info_fetch_pos(mi, 1, &ls, &le);
            g_match_info_fetch_pos(mi, 2, &rs, &re2);
            if (ls >= 0 && le > ls) {
                GtkTextIter a, b;
                glong cl = base_offset + g_utf8_pointer_to_offset(text, text + ls);
                glong cr = base_offset + g_utf8_pointer_to_offset(text, text + le);
                gtk_text_buffer_get_iter_at_offset(buf, &a, (gint)cl);
                gtk_text_buffer_get_iter_at_offset(buf, &b, (gint)cr);
                gtk_text_buffer_apply_tag(buf, app->hl_tag_dot_l, &a, &b);
            }
            if (rs >= 0 && re2 > rs) {
                GtkTextIter a, b;
                glong cl = base_offset + g_utf8_pointer_to_offset(text, text + rs);
                glong cr = base_offset + g_utf8_pointer_to_offset(text, text + re2);
                gtk_text_buffer_get_iter_at_offset(buf, &a, (gint)cl);
                gtk_text_buffer_get_iter_at_offset(buf, &b, (gint)cr);
                gtk_text_buffer_apply_tag(buf, app->hl_tag_dot_r, &a, &b);
            }
            g_match_info_next(mi, NULL);
        }
        g_match_info_free(mi);
    }
    /* comments — highest priority, painted last */
    {
        static GRegex *re_cmt = NULL;
        if (!re_cmt) re_cmt = g_regex_new("(#[^\n]*|//[^\n]*)", G_REGEX_OPTIMIZE, 0, NULL);
        hl_apply_regex(buf, re_cmt, app->hl_tag_comment, text, 0, base_offset);
    }

    g_free(text);
}

static void hl_apply_lines(AppState *app, EditorTab *tab) {
    if (!app->hl_enabled || !tab->dirty_lines) return;
    GArray *a = tab->dirty_lines;
    for (guint i = 0; i < a->len; i++)
        hl_apply_line(app, tab, g_array_index(a, gint, i));
    g_array_set_size(a, 0);  /* clear dirty list */
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

    /* piper executable and voice model from config */
    const char *piper_path = app->tts_piper_path
        ? app->tts_piper_path : "/home/cruxible/pyra_env/bin/piper";
    const char *model_dir  = app->tts_model_dir
        ? app->tts_model_dir  : "/home/cruxible/cyon/piper_models";
    const char *model_file = app->tts_voice_joe
        ? (app->tts_model_joe    ? app->tts_model_joe    : "en_US-joe-medium.onnx")
        : (app->tts_model_lessac ? app->tts_model_lessac : "en_US-lessac-medium.onnx");

    char voice_model[PATH_MAX];
    snprintf(voice_model, sizeof(voice_model), "%s/%s", model_dir, model_file);

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

static void on_destroy(GtkWidget *w, AppState *app);  /* forward */

/* ── app_load_config ────────────────────────────────────────────────────── */

static void app_load_config(AppState *app) {
    char *path = get_config_path();

    GKeyFile *kf  = g_key_file_new();
    GError   *err = NULL;
    g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, &err);
    if (err) { g_error_free(err); err = NULL; }
    g_free(path);

    /* ── [editor] ── */
    gint font = g_key_file_get_integer(kf, "editor", "font_size", &err);
    if (!err && font >= MIN_FONT_PX && font <= MAX_FONT_PX)
        app->font_size = font;
    else { g_clear_error(&err); }

    /* tree_visible and pane position applied later in build_ui after widgets exist */
    /* stored on app so build_ui can read them */
    app->_cfg_tree_visible  = !g_key_file_get_boolean(kf, "editor", "tree_hidden", NULL);
    gint pane_pos = g_key_file_get_integer(kf, "editor", "tree_pane_position", &err);
    app->_cfg_pane_pos = (!err && pane_pos > 0) ? pane_pos : 600;
    g_clear_error(&err);

    gint win_w = g_key_file_get_integer(kf, "editor", "window_width",  &err); g_clear_error(&err);
    gint win_h = g_key_file_get_integer(kf, "editor", "window_height", &err); g_clear_error(&err);
    app->_cfg_win_w = (win_w > 200) ? win_w : 900;
    app->_cfg_win_h = (win_h > 200) ? win_h : 660;

    /* ── [tts] ── */
    char *pp = g_key_file_get_string(kf, "tts", "piper_path", NULL);
    app->tts_piper_path = (pp && pp[0]) ? pp : g_strdup("/home/cruxible/pyra_env/bin/piper");

    char *md = g_key_file_get_string(kf, "tts", "model_dir", NULL);
    app->tts_model_dir = (md && md[0]) ? md : g_strdup("/home/cruxible/cyon/piper_models");

    char *mj = g_key_file_get_string(kf, "tts", "voice_joe_model", NULL);
    app->tts_model_joe = (mj && mj[0]) ? mj : g_strdup("en_US-joe-medium.onnx");

    char *ml = g_key_file_get_string(kf, "tts", "voice_lessac_model", NULL);
    app->tts_model_lessac = (ml && ml[0]) ? ml : g_strdup("en_US-lessac-medium.onnx");

    char *voice = g_key_file_get_string(kf, "tts", "voice", NULL);
    if (voice && strcmp(voice, "lessac") == 0) app->tts_voice_joe = FALSE;
    g_free(voice);

    /* ── [session] ── */
    char *notes_dir_cfg = g_key_file_get_string(kf, "session", "notes_dir", NULL);
    if (notes_dir_cfg && notes_dir_cfg[0]) {
        /* expand leading ~ */
        if (notes_dir_cfg[0] == '~') {
            const char *home = g_get_home_dir();
            app->_cfg_notes_dir = g_strconcat(home, notes_dir_cfg + 1, NULL);
            g_free(notes_dir_cfg);
        } else {
            app->_cfg_notes_dir = notes_dir_cfg;
        }
    } else {
        g_free(notes_dir_cfg);
        app->_cfg_notes_dir = NULL;
    }

    gsize  n_files = 0;
    gchar **open_files = g_key_file_get_string_list(kf, "session", "open_files", &n_files, NULL);
    app->_cfg_open_files   = open_files;
    app->_cfg_open_files_n = (int)n_files;

    gint active = g_key_file_get_integer(kf, "session", "active_tab", &err);
    app->_cfg_active_tab = (!err && active >= 0) ? active : 0;
    g_clear_error(&err);

    /* ── [ram_graph] ── */
    static const double DEF_LINE[3] = { 0.910, 0.627, 0.125 }; /* amber */
    static const double DEF_GLOW[3] = { 0.910, 0.627, 0.125 }; /* amber */
    static const double DEF_GRID[3] = { 0.000, 0.600, 0.300 }; /* green */
    static const double DEF_TEXT[3] = { 0.000, 1.000, 0.600 }; /* bright green */
    char *rc_line = g_key_file_get_string(kf, "ram_graph", "color_line", NULL);
    char *rc_glow = g_key_file_get_string(kf, "ram_graph", "color_glow", NULL);
    char *rc_grid = g_key_file_get_string(kf, "ram_graph", "color_grid", NULL);
    char *rc_text = g_key_file_get_string(kf, "ram_graph", "color_text", NULL);
    parse_hex_color(rc_line, app->ram_col_line, DEF_LINE);
    parse_hex_color(rc_glow, app->ram_col_glow, DEF_GLOW);
    parse_hex_color(rc_grid, app->ram_col_grid, DEF_GRID);
    parse_hex_color(rc_text, app->ram_col_text, DEF_TEXT);
    g_free(rc_line); g_free(rc_glow); g_free(rc_grid); g_free(rc_text);

    g_key_file_free(kf);
}

/* ── app_save_session ───────────────────────────────────────────────────── */

static void app_save_session(AppState *app) {
    char *path = get_config_path();

    /* load existing file so we preserve highlight sections */
    GKeyFile *kf  = g_key_file_new();
    GError   *err = NULL;
    g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, &err);
    if (err) { g_error_free(err); err = NULL; }

    /* ── [editor] ── */
    g_key_file_set_integer(kf, "editor", "font_size", app->font_size);

    gboolean tree_vis = gtk_widget_get_visible(app->tree_panel);
    g_key_file_set_boolean(kf, "editor", "tree_hidden", !tree_vis);

    if (app->paned)
        g_key_file_set_integer(kf, "editor", "tree_pane_position",
                               gtk_paned_get_position(GTK_PANED(app->paned)));

    gint win_w = (app->last_win_w >= 800) ? app->last_win_w : 900;
    gint win_h = (app->last_win_h >= 300) ? app->last_win_h : 660;
    g_key_file_set_integer(kf, "editor", "window_width",  win_w);
    g_key_file_set_integer(kf, "editor", "window_height", win_h);

    /* ── [tts] ── */
    if (app->tts_piper_path)   g_key_file_set_string(kf, "tts", "piper_path",        app->tts_piper_path);
    if (app->tts_model_dir)    g_key_file_set_string(kf, "tts", "model_dir",          app->tts_model_dir);
    if (app->tts_model_joe)    g_key_file_set_string(kf, "tts", "voice_joe_model",    app->tts_model_joe);
    if (app->tts_model_lessac) g_key_file_set_string(kf, "tts", "voice_lessac_model", app->tts_model_lessac);
    g_key_file_set_string(kf, "tts", "voice", app->tts_voice_joe ? "joe" : "lessac");

    /* ── [session] ── */
    /* notes dir — use tree_folder if set, else default */
    const char *ndir = app->tree_folder ? app->tree_folder : "";
    g_key_file_set_string(kf, "session", "notes_dir", ndir);

    /* collect open file paths from all tabs */
    int n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook));
    GPtrArray *files = g_ptr_array_new();
    for (int i = 0; i < n_pages; i++) {
        GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(app->notebook), i);
        EditorTab *tab  = g_object_get_data(G_OBJECT(page), "editor-tab");
        if (tab && tab->current_file)
            g_ptr_array_add(files, tab->current_file);
    }
    if (files->len > 0)
        g_key_file_set_string_list(kf, "session", "open_files",
                                   (const gchar *const *)files->pdata, files->len);
    else
        g_key_file_set_string_list(kf, "session", "open_files", NULL, 0);
    g_ptr_array_free(files, FALSE);

    gint active_idx = gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook));
    g_key_file_set_integer(kf, "session", "active_tab", active_idx);

    /* ── [ram_graph] — write if keys missing so first-run gets the section ── */
    {
        char hex[8];
        if (!g_key_file_has_key(kf, "ram_graph", "color_line", NULL)) {
            g_snprintf(hex, sizeof(hex), "#%02X%02X%02X",
                (int)(app->ram_col_line[0]*255), (int)(app->ram_col_line[1]*255), (int)(app->ram_col_line[2]*255));
            g_key_file_set_string(kf, "ram_graph", "color_line", hex);
        }
        if (!g_key_file_has_key(kf, "ram_graph", "color_glow", NULL)) {
            g_snprintf(hex, sizeof(hex), "#%02X%02X%02X",
                (int)(app->ram_col_glow[0]*255), (int)(app->ram_col_glow[1]*255), (int)(app->ram_col_glow[2]*255));
            g_key_file_set_string(kf, "ram_graph", "color_glow", hex);
        }
        if (!g_key_file_has_key(kf, "ram_graph", "color_grid", NULL)) {
            g_snprintf(hex, sizeof(hex), "#%02X%02X%02X",
                (int)(app->ram_col_grid[0]*255), (int)(app->ram_col_grid[1]*255), (int)(app->ram_col_grid[2]*255));
            g_key_file_set_string(kf, "ram_graph", "color_grid", hex);
        }
        if (!g_key_file_has_key(kf, "ram_graph", "color_text", NULL)) {
            g_snprintf(hex, sizeof(hex), "#%02X%02X%02X",
                (int)(app->ram_col_text[0]*255), (int)(app->ram_col_text[1]*255), (int)(app->ram_col_text[2]*255));
            g_key_file_set_string(kf, "ram_graph", "color_text", hex);
        }
    }

    /* write back */
    gsize  len  = 0;
    gchar *data = g_key_file_to_data(kf, &len, NULL);
    if (data) {
        g_file_set_contents(path, data, (gssize)len, NULL);
        g_free(data);
    }
    g_key_file_free(kf);
    g_free(path);
}

/* ── on_destroy ─────────────────────────────────────────────────────────── */

static void on_destroy(GtkWidget *w, AppState *app) {
    if (app->tts_pid) {
        kill(app->tts_pid, SIGTERM);
        g_spawn_close_pid(app->tts_pid);
    }
    app_save_session(app);
    g_free(app->tree_folder);
    g_free(app->tts_piper_path);
    g_free(app->tts_model_dir);
    g_free(app->tts_model_joe);
    g_free(app->tts_model_lessac);
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

    char *root_label = g_strdup_printf("◎ %s", safe_root_name);

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

    if (!fpath || !g_file_test(fpath, G_FILE_TEST_IS_REGULAR)) {
        g_free(fpath);
        return;
    }

    /* If already open in a tab, just switch to it — avoids reload+scroll jump */
    int n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook));
    for (int i = 0; i < n_pages; i++) {
        GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(app->notebook), i);
        EditorTab *tab  = g_object_get_data(G_OBJECT(page), "editor-tab");
        if (tab && tab->current_file && strcmp(tab->current_file, fpath) == 0) {
            gtk_notebook_set_current_page(GTK_NOTEBOOK(app->notebook), i);
            g_free(fpath);
            return;
        }
    }

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

static gboolean on_window_configure(GtkWidget *w, GdkEventConfigure *ev,
                                    AppState *app) {
    /* cache the live window size — used by app_save_session on exit */
    app->last_win_w = ev->width;
    app->last_win_h = ev->height;
    return FALSE;  /* propagate */
}





/* ── hex color parser ───────────────────────────────────────────────────── */

static void parse_hex_color(const char *hex, double out[3], const double def[3]) {
    if (!hex || hex[0] != '#' || strlen(hex) < 7) {
        out[0] = def[0]; out[1] = def[1]; out[2] = def[2]; return;
    }
    unsigned int r, g, b;
    if (sscanf(hex + 1, "%02x%02x%02x", &r, &g, &b) == 3) {
        out[0] = r / 255.0; out[1] = g / 255.0; out[2] = b / 255.0;
    } else {
        out[0] = def[0]; out[1] = def[1]; out[2] = def[2];
    }
}

/* ══════════════════════════════════════════════════════════════════════════
 * FEATURE: RAM usage graph  (scrolling Cairo line, amber/green)
 * ══════════════════════════════════════════════════════════════════════════ */

#define RAM_SAMPLES 120

static float ram_read_mb(void) {
    FILE *f = fopen("/proc/self/status", "r");
    if (!f) return 0.f;
    char line[128];
    float kb = 0.f;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, "%f", &kb);
            break;
        }
    }
    fclose(f);
    return kb / 1024.f;
}

static gboolean on_ram_tick(AppState *app) {
    float mb = ram_read_mb();
    app->ram_samples[app->ram_head] = mb;
    app->ram_head = (app->ram_head + 1) % RAM_SAMPLES;
    if (app->ram_count < RAM_SAMPLES) app->ram_count++;
    gtk_widget_queue_draw(app->ram_graph);
    return G_SOURCE_CONTINUE;
}

static gboolean on_ram_draw(GtkWidget *widget, cairo_t *cr, AppState *app) {
    int w = gtk_widget_get_allocated_width(widget);
    int h = gtk_widget_get_allocated_height(widget);

    /* background */
    cairo_set_source_rgb(cr, 0.012, 0.012, 0.02);
    cairo_paint(cr);

    int n = app->ram_count;
    if (n < 2) return FALSE;

    /* find min/max for scaling */
    float mn = 1e9f, mx = 0.f;
    for (int i = 0; i < n; i++) {
        int idx = (app->ram_head - n + i + RAM_SAMPLES) % RAM_SAMPLES;
        float v = app->ram_samples[idx];
        if (v < mn) mn = v;
        if (v > mx) mx = v;
    }
    float range = mx - mn;
    if (range < 1.f) range = 1.f;  /* avoid flat-line divide */

    /* grid lines */
    cairo_set_line_width(cr, 0.5);
    cairo_set_source_rgba(cr, app->ram_col_grid[0], app->ram_col_grid[1], app->ram_col_grid[2], 0.18);
    for (int g = 1; g < 4; g++) {
        double gy = h - (h * g / 4.0);
        cairo_move_to(cr, 0, gy);
        cairo_line_to(cr, w, gy);
        cairo_stroke(cr);
    }

    /* glow pass — wider, low alpha */
    cairo_set_line_width(cr, 3.0);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_source_rgba(cr, app->ram_col_glow[0], app->ram_col_glow[1], app->ram_col_glow[2], 0.25);
    for (int i = 0; i < n; i++) {
        int idx = (app->ram_head - n + i + RAM_SAMPLES) % RAM_SAMPLES;
        float v = app->ram_samples[idx];
        double x = (double)i / (n - 1) * w;
        double y = h - ((v - mn) / range) * (h - 4) - 2;
        if (i == 0) cairo_move_to(cr, x, y);
        else        cairo_line_to(cr, x, y);
    }
    cairo_stroke(cr);

    /* main line */
    cairo_set_line_width(cr, 1.5);
    cairo_set_source_rgb(cr, app->ram_col_line[0], app->ram_col_line[1], app->ram_col_line[2]);
    for (int i = 0; i < n; i++) {
        int idx = (app->ram_head - n + i + RAM_SAMPLES) % RAM_SAMPLES;
        float v = app->ram_samples[idx];
        double x = (double)i / (n - 1) * w;
        double y = h - ((v - mn) / range) * (h - 4) - 2;
        if (i == 0) cairo_move_to(cr, x, y);
        else        cairo_line_to(cr, x, y);
    }
    cairo_stroke(cr);

    /* current value label */
    float cur = app->ram_samples[(app->ram_head - 1 + RAM_SAMPLES) % RAM_SAMPLES];
    char lbl[32];
    g_snprintf(lbl, sizeof(lbl), "%.1f MB", cur);
    cairo_set_source_rgb(cr, app->ram_col_text[0], app->ram_col_text[1], app->ram_col_text[2]);
    cairo_select_font_face(cr, "monospace",
        CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 9.0);
    cairo_move_to(cr, 4, h - 4);
    cairo_show_text(cr, lbl);

    return FALSE;
}

/* ── highlight toggle ───────────────────────────────────────────────────── */

static void on_hl_toggle(GtkButton *btn, AppState *app) {
    app->hl_enabled = !app->hl_enabled;
    gtk_button_set_label(btn, app->hl_enabled ? "HL ON" : "HL OFF");

    if (app->hl_enabled) {
        /* Save scroll positions for all tabs before triggering highlight —
         * tag apply/remove signals cause GTK to scroll-to-cursor internally */
        int n = gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook));
        gdouble saved[64] = {0};  /* save up to 64 tabs */
        GtkAdjustment *adjs[64]  = {NULL};
        for (int i = 0; i < n && i < 64; i++) {
            GtkWidget *pg = gtk_notebook_get_nth_page(GTK_NOTEBOOK(app->notebook), i);
            EditorTab *t  = g_object_get_data(G_OBJECT(pg), "editor-tab");
            if (!t) continue;
            adjs[i]  = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(t->scroll));
            saved[i] = gtk_adjustment_get_value(adjs[i]);
            /* use full highlight pass so hl_deferred_cb's scroll restore fires */
            t->hl_full_pending = TRUE;
            if (t->hl_timer) { g_source_remove(t->hl_timer); t->hl_timer = 0; }
            t->hl_timer = g_timeout_add(80, (GSourceFunc)hl_deferred_cb, t);
        }
        /* Also restore from here as a belt-and-suspenders fallback */
        for (int i = 0; i < n && i < 64; i++) {
            if (!adjs[i]) continue;
            g_idle_add(hl_scroll_restore_cb, make_scroll_data(adjs[i], saved[i]));
            g_timeout_add(160, hl_scroll_restore_cb, make_scroll_data(adjs[i], saved[i]));
        }
    } else {
        /* strip all highlight tags — save/restore scroll since remove_tag
         * also triggers internal scroll-to-cursor */
        int n = gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook));
        for (int i = 0; i < n; i++) {
            GtkWidget  *pg  = gtk_notebook_get_nth_page(GTK_NOTEBOOK(app->notebook), i);
            EditorTab  *t   = g_object_get_data(G_OBJECT(pg), "editor-tab");
            if (!t) continue;
            GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(
                GTK_SCROLLED_WINDOW(t->scroll));
            gdouble saved = gtk_adjustment_get_value(vadj);
            GtkTextIter s, e;
            gtk_text_buffer_get_bounds(t->text_buf, &s, &e);
            for (int j = 0; j < app->hl_group_count; j++)
                if (app->hl_kw_tags[j])
                    gtk_text_buffer_remove_tag(t->text_buf, app->hl_kw_tags[j], &s, &e);
            if (app->hl_tag_lime)    gtk_text_buffer_remove_tag(t->text_buf, app->hl_tag_lime,    &s, &e);
            if (app->hl_tag_coral)   gtk_text_buffer_remove_tag(t->text_buf, app->hl_tag_coral,   &s, &e);
            if (app->hl_tag_comment) gtk_text_buffer_remove_tag(t->text_buf, app->hl_tag_comment, &s, &e);
            if (app->hl_tag_dot_l)   gtk_text_buffer_remove_tag(t->text_buf, app->hl_tag_dot_l,   &s, &e);
            if (app->hl_tag_dot_r)   gtk_text_buffer_remove_tag(t->text_buf, app->hl_tag_dot_r,   &s, &e);
            if (app->hl_tag_steel)   gtk_text_buffer_remove_tag(t->text_buf, app->hl_tag_steel,   &s, &e);
            g_idle_add(hl_scroll_restore_cb, make_scroll_data(vadj, saved));
            g_timeout_add(80, hl_scroll_restore_cb, make_scroll_data(vadj, saved));
        }
    }
    app_log(app, "▸ Highlighting %s.", app->hl_enabled ? "on" : "off");
}

/* ── README / about window ──────────────────────────────────────────────── */

static const char *README_TEXT =
"╔══════════════════════════════════════════════════════════════╗\n"
"║          CYON NOTES  //  TTS  //  README.txt                 ║\n"
"║          by Ioannes Cruxibulum                               ║\n"
"║          est. 2023  |  still compiling  |  send help         ║\n"
"╚══════════════════════════════════════════════════════════════╝\n"
"\n"
"  welcome to cyon_notes. you didn't ask for it.\n"
"  it's here anyway. buckle up.\n"
"\n"
"──────────────────────────────────────────────────────────────\n"
" WHAT IS THIS\n"
"──────────────────────────────────────────────────────────────\n"
"\n"
"  a text editor. yes, another one. no, it's not VS Code.\n"
"  no, it's not Sublime. those are for people who like\n"
"  paying for things or waiting for electron to load.\n"
"  this is GTK3, C, and raw willpower.\n"
"\n"
"  it was built from scratch over 5 years as part of the\n"
"  CYON desktop environment. cyon sits on top. pyra is\n"
"  underneath. pyra is where the dragons live.\n"
"  you are currently in cyon. you are safe. probably.\n"
"\n"
"──────────────────────────────────────────────────────────────\n"
" FEATURES  (they work, we checked, mostly)\n"
"──────────────────────────────────────────────────────────────\n"
"\n"
"  TABS\n"
"    multiple tabs. yes. we live in 2024.\n"
"    Ctrl+T = new tab.  click the X to close one.\n"
"    reorderable via drag and drop like a civilized human.\n"
"\n"
"  SAVING\n"
"    Ctrl+S saves. if you don't Ctrl+S you will lose your work.\n"
"    this is not a bug. this is a life lesson.\n"
"    files save as .txt. the extension is added automatically.\n"
"    you're welcome.\n"
"\n"
"  UNDO / REDO\n"
"    Ctrl+Z = undo.  Ctrl+Y = redo.\n"
"    up to 300 undo steps. if you need more than 300 undos\n"
"    that's a you problem, not a cyon_notes problem.\n"
"\n"
"  SYNTAX HIGHLIGHTING\n"
"    press HL OFF to turn it on. yes the button says OFF.\n"
"    that means highlighting is currently OFF and you can\n"
"    turn it ON by clicking the button that says OFF.\n"
"    this confused everyone. this comment exists because of that.\n"
"    supports: keywords, strings, comments, dot notation,\n"
"    class names, = operators, @staticmethod.\n"
"    configured via cyon_notes.conf. go nuts.\n"
"\n"
"  LINE NUMBERS\n"
"    they're on the left. they count up. that's their whole job.\n"
"    they're very good at it.\n"
"\n"
"  AUTO-INDENT\n"
"    press Enter after a colon and it indents for you.\n"
"    press Backspace on an indent and it un-indents.\n"
"    like a real editor. because this is a real editor.\n"
"\n"
"  AUTO-CLOSE BRACKETS\n"
"    type ( and get (). type [ and get []. type { and get {}.\n"
"    type ' and get ''. type \" and get \"\".\n"
"    cursor lands in the middle, ready to type.\n"
"    if you hate this: ctrl+z immediately after. it leaves.\n"
"\n"
"  CTRL+/ TOGGLE COMMENT\n"
"    select lines, hit Ctrl+/. they become # comments.\n"
"    hit Ctrl+/ again. they stop being # comments.\n"
"    works on single lines too. very efficient.\n"
"    very 10x developer energy.\n"
"\n"
"  CTRL+D DUPLICATE LINE\n"
"    duplicates the current line downward.\n"
"    use responsibly. or don't. we're a README not a cop.\n"
"\n"
"  CTRL+SCROLL ZOOM\n"
"    scroll while holding Ctrl to resize text.\n"
"    range: 8px to 36px. for when your eyes need a break\n"
"    or you're presenting to someone across the room.\n"
"\n"
"  FILE TREE\n"
"    the panel on the right. shows your files.\n"
"    double-click to open. if the file is already open\n"
"    it switches to that tab instead of opening a duplicate.\n"
"    because we're not animals.\n"
"    click FOLDER to point it at a directory.\n"
"    click the refresh button when the tree lies to you.\n"
"    HIDE TREE in the FILE menu if you need the space.\n"
"\n"
"  TTS  (text to speech)\n"
"    requires piper + aplay. if you don't have those,\n"
"    the buttons still work, they just do nothing.\n"
"    select some text, click SELECTION. it reads it.\n"
"    click ALL and it reads the whole file.\n"
"    click STOP when you've heard enough.\n"
"    JOE = male voice.  LESSAC = female voice.\n"
"    configure paths in cyon_notes.conf.\n"
"\n"
"  RAM GRAPH\n"
"    bottom right corner. little graph. watches your RAM.\n"
"    updates every second. purely for vibes.\n"
"    green line = current usage. amber glow = style.\n"
"    if the line goes to the top: close some tabs, maybe.\n"
"\n"
"  SESSION RESTORE\n"
"    when you close and reopen, your tabs come back.\n"
"    window size, pane position, active tab — all restored.\n"
"    like nothing ever happened. very chill.\n"
"\n"
"  SINGLE INSTANCE LOCK\n"
"    try to open cyon_notes twice. go ahead.\n"
"    it will tell you no. one instance only.\n"
"    this is a feature. you only need one.\n"
"\n"
"──────────────────────────────────────────────────────────────\n"
" KNOWN ISSUES\n"
"──────────────────────────────────────────────────────────────\n"
"\n"
"  - the scroll position sometimes flickers when you click.\n"
"    this was a bug. now it's a feature. -- cruxibulum\n"
"\n"
"  - the HL button says OFF when highlighting is OFF.\n"
"    this is technically correct. the worst kind of correct.\n"
"\n"
"  - if pyra_lib is missing, pyra_toolz will tell you.\n"
"    cyon_notes doesn't care. cyon_notes stands alone.\n"
"\n"
"──────────────────────────────────────────────────────────────\n"
" KEYBOARD SHORTCUTS  (the ones worth knowing)\n"
"──────────────────────────────────────────────────────────────\n"
"\n"
"  Ctrl+S        save\n"
"  Ctrl+Z        undo\n"
"  Ctrl+Y        redo\n"
"  Ctrl+T        new tab\n"
"  Ctrl+/        toggle comment\n"
"  Ctrl+D        duplicate line\n"
"  Ctrl+scroll   zoom text\n"
"  Tab           indent\n"
"  Shift+Tab     unindent\n"
"\n"
"──────────────────────────────────────────────────────────────\n"
" CONFIGURATION  (cyon_notes.conf)\n"
"──────────────────────────────────────────────────────────────\n"
"\n"
"  lives in ~/cyon/src/cyon_notes.conf\n"
"  sections: [editor] [tts] [session] [ram_graph]\n"
"  and one or more [highlight_group_N] sections.\n"
"  edit it with any text editor. including this one.\n"
"  very meta. very cyon.\n"
"\n"
"──────────────────────────────────────────────────────────────\n"
" DEPENDENCIES  (congrats, you already have them)\n"
"──────────────────────────────────────────────────────────────\n"
"\n"
"  if you are reading this, the program compiled and ran.\n"
"  which means you already have everything listed below.\n"
"  this section exists purely for completeness.\n"
"  and to make the README longer. mostly that.\n"
"\n"
"  GTK3\n"
"    sudo apt install libgtk-3-dev\n"
"    (you have this. we know. you know. everyone knows.)\n"
"\n"
"  GLib\n"
"    comes with GTK3. you didn't even have to ask.\n"
"    very polite library.\n"
"\n"
"  GCC\n"
"    sudo apt install build-essential\n"
"    (it compiled. gcc was there. this is not news.)\n"
"\n"
"  piper TTS  (optional, for the TTS buttons to do anything)\n"
"    pip install piper-tts\n"
"    voice models go in ~/cyon/piper_models/\n"
"    without this: TTS buttons sit there looking pretty.\n"
"    they're fine with it.\n"
"\n"
"  aplay  (optional, also for TTS)\n"
"    sudo apt install alsa-utils\n"
"    piper pipes into aplay. no aplay, no audio.\n"
"    very logical. very unix.\n"
"\n"
"  to build cyon_notes from source:\n"
"    gcc $(pkg-config --cflags gtk+-3.0) -o cyon_notes cyon_notes.c \\\n"
"         $(pkg-config --libs gtk+-3.0) -lpthread\n"
"\n"
"  if that command means nothing to you: ask someone.\n"
"  if it worked: you're already in the program. hi.\n"
"\n"
"──────────────────────────────────────────────────────────────\n"
" CREDITS\n"
"──────────────────────────────────────────────────────────────\n"
"\n"
"  written by Ioannes Cruxibulum\n"
"  built on: GTK3, GLib, GRegex, piper TTS, pure stubbornness\n"
"  inspired by: needing a good editor and not finding one\n"
"  powered by: caffeine, spite, and 5 years of iteration\n"
"  tested on: Linux  (other OSes: not our problem)\n"
"\n"
"  special thanks to:\n"
"    - the GTK documentation, for existing\n"
"    - the GTK documentation, for being confusing enough\n"
"      to make this take 5 years\n"
"    - pikachu, for the error messages in pyra_toolz\n"
"    - whoever invented Ctrl+Z\n"
"\n"
"  this software is provided as-is. if it breaks something\n"
"  you probably deserved it. if it doesn't break anything\n"
"  you're welcome.\n"
"\n"
"  -- cruxibulum  //  cyon project  //  est. 2023\n"
"\n"
"  [press OK to close this and get back to work]\n";


/* ── boot sequence ───────────────────────────────────────────────────────── */

static const char *BOOT_QUOTES[] = {
    "▸ no bugs found. (we stopped looking.)",
    "▸ piper is ready. whether you are is your problem.",
    "▸ still compiling. always compiling. somewhere.",
    "▸ GTK3: because GTK4 is someone else's problem.",
    "▸ syntax highlighting: OFF by default. you know why.",
    "▸ this editor replaced Sublime. Sublime doesn't know yet.",
    "▸ 5 years in the making. 0 regrets. maybe 1.",
    "▸ warning: may cause productivity.",
    "▸ cyon_notes.conf: edit it. you'll know what to do.",
    "▸ the RAM graph is purely for vibes. important vibes.",
    "▸ single instance only. we trust you. mostly.",
    "▸ undo stack: 300 deep. if you need more, call someone.",
    "▸ TTS voices: joe (male) lessac (female) you (optional).",
    "▸ welcome back. the code missed you. probably.",
    "▸ pyra is underneath. do not go there unless you must.",
    "▸ ctrl+z is in there. you're welcome. use it wisely.",
    "▸ the scroll flicker was a bug. now it's a feature. -- cruxibulum",
    "▸ linux only. other OSes: not our problem.",
    "▸ file saved. probably. hit Ctrl+S anyway.",
    "▸ cyon: built from scratch. held together by stubbornness.",
};
#define N_BOOT_QUOTES ((int)(sizeof(BOOT_QUOTES) / sizeof(BOOT_QUOTES[0])))

/* boot steps: label + bar fill (out of 10) */
typedef struct {
    const char *label;
    int         fill;
} BootStep;

static const BootStep BOOT_STEPS[] = {
    { "initializing cyon_notes",        2  },
    { "loading config",                 3  },
    { "warming up GTK",                 4  },
    { "checking for bugs",              5  },
    { "none found (we stopped looking)",6  },
    { "mounting file tree",             7  },
    { "arming syntax engine",           8  },
    { "bribing the RAM graph",          9  },
    { "restoring session",              10 },
};
#define N_BOOT_STEPS ((int)(sizeof(BOOT_STEPS) / sizeof(BOOT_STEPS[0])))

typedef struct {
    AppState *app;
    int       step;
} BootState;

static char *make_bar(int fill, int total) {
    /* e.g. [████████░░] */
    GString *s = g_string_new("[");
    for (int i = 0; i < total; i++)
        g_string_append(s, i < fill ? "\xe2\x96\x88" : "\xe2\x96\x91");
    g_string_append(s, "]");
    return g_string_free(s, FALSE);
}

/* boot_step_cb forward — called by boot_sequence before it is defined */
static gboolean boot_step_cb(gpointer data);

static void boot_sequence(AppState *app) {
    app_log(app, "▸ CYON NOTES // booting...");
    app_log(app, " ");
    BootState *bs = g_new(BootState, 1);
    bs->app  = app;
    bs->step = 0;
    g_timeout_add(80, boot_step_cb, bs);
}

static gboolean boot_sequence_once(gpointer data) {
    boot_sequence((AppState *)data);
    return G_SOURCE_REMOVE;
}

static gboolean boot_step_cb(gpointer data) {
    BootState *bs = data;
    AppState  *app = bs->app;
    int        step = bs->step;
    g_free(bs);

    if (step < N_BOOT_STEPS) {
        char *bar = make_bar(BOOT_STEPS[step].fill, 10);
        app_log(app, "  %s %s", bar, BOOT_STEPS[step].label);
        g_free(bar);

        BootState *next = g_new(BootState, 1);
        next->app  = app;
        next->step = step + 1;
        g_timeout_add(120, boot_step_cb, next);
    } else {
        /* all steps done — print final bar + random quote then STOP */
        char *bar = make_bar(10, 10);
        app_log(app, "  %s done.", bar);
        g_free(bar);
        app_log(app, " ");
        app_log_amber(app, "%s", BOOT_QUOTES[g_random_int_range(0, N_BOOT_QUOTES)]);
        app_log(app, " ");
        app_log(app, "▸ Ready.");
    }
    return G_SOURCE_REMOVE;
}

static void on_readme(GtkMenuItem *item, AppState *app) {
    GtkWidget *dlg = gtk_dialog_new_with_buttons(
        "CYON NOTES // README",
        GTK_WINDOW(app->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "OK", GTK_RESPONSE_OK,
        NULL);

    gtk_window_set_default_size(GTK_WINDOW(dlg), 680, 520);

    /* style the dialog to match cyon */
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
        "dialog, dialog > box { background-color: #05050a; }"
        "dialog > box > box > button {"
        "  background-color: #0a1a10; color: #00ff99;"
        "  font-family: monospace; font-size: 11px;"
        "  border: 1px solid #1a4a28; border-radius: 2px; padding: 3px 18px; }"
        "dialog > box > box > button:hover { border-color: #E8A020; }"
        "dialog textview, dialog textview text {"
        "  background-color: #05050a; color: #00cc77;"
        "  font-family: monospace; font-size: 12px; }"
        "dialog scrolledwindow { border: 1px solid #1a2a20; }",
        -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gtk_widget_get_screen(dlg),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);

    /* scrolled text view */
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_margin_start(scroll, 10);
    gtk_widget_set_margin_end(scroll, 10);
    gtk_widget_set_margin_top(scroll, 10);
    gtk_widget_set_margin_bottom(scroll, 6);

    GtkWidget *tv = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(tv), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(tv), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(tv), GTK_WRAP_NONE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(tv), 10);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(tv), 10);

    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));
    gtk_text_buffer_set_text(buf, README_TEXT, -1);

    gtk_container_add(GTK_CONTAINER(scroll), tv);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    gtk_box_pack_start(GTK_BOX(content), scroll, TRUE, TRUE, 0);
    gtk_widget_show_all(dlg);

    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
}

static void build_ui(AppState *app) {
    /* ── window ───────────────────────────────────────────────────── */
    app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app->window), APP_TITLE);

    gint init_w = (app->_cfg_win_w >= 800) ? app->_cfg_win_w : 900;
    gint init_h = (app->_cfg_win_h >= 300) ? app->_cfg_win_h : 660;
    gtk_window_set_default_size(GTK_WINDOW(app->window), init_w, init_h);
    app->last_win_w = init_w;
    app->last_win_h = init_h;

    gtk_container_set_border_width(GTK_CONTAINER(app->window), 10);
    g_signal_connect(app->window, "destroy", G_CALLBACK(on_destroy), app);
    g_signal_connect(app->window, "configure-event",
                     G_CALLBACK(on_window_configure), app);

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
    GtkWidget *title = gtk_label_new("⬡ CYON DEV NOTES // TTS [WARN]                        ☢ GtkLabel: Exists only to judge you silently. ☢ ");
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
        { "◆ NEW TAB",       G_CALLBACK(on_new_tab)      },
        { "◆ NEW",           G_CALLBACK(on_new)           },
        { "◆ LOAD",          G_CALLBACK(on_load)          },
        { "◆ SAVE",          G_CALLBACK(on_save)          },
        { "◆ SAVE AS",       G_CALLBACK(on_save_as)       },
        { NULL,            NULL                         },
        { "◆ UNDO  Ctrl+Z",  G_CALLBACK(on_undo)          },
        { "◆ REDO  Ctrl+Y",  G_CALLBACK(on_redo)          },
        { NULL,            NULL                         },
        { "◆ DELETE FILE",   G_CALLBACK(on_delete_file)   },
        { NULL,            NULL                         },
        { "◆ TEXT  +",       G_CALLBACK(on_text_size_inc) },
        { "◆ TEXT  −",       G_CALLBACK(on_text_size_dec) },
        { NULL,            NULL                         },
        { "◆ HIDE TREE",     G_CALLBACK(on_tree_toggle)   },
        { NULL,            NULL                         },
        { "◆ README",        G_CALLBACK(on_readme)        },
    };
    for (size_t i = 0; i < G_N_ELEMENTS(menu_items); i++) {
        if (!menu_items[i].label) {
            gtk_menu_shell_append(GTK_MENU_SHELL(app->file_menu),
                gtk_separator_menu_item_new());
        } else {
            GtkWidget *it = make_menu_item(
                menu_items[i].label, menu_items[i].cb, app);
            gtk_menu_shell_append(GTK_MENU_SHELL(app->file_menu), it);
            if (strcmp(menu_items[i].label, "◆ HIDE TREE") == 0)
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
        { "◆ JOE",         G_CALLBACK(on_tts_voice_joe)       },
        { "◆ LESSAC",      G_CALLBACK(on_tts_voice_lessac)    },
    };
    for (size_t i = 0; i < G_N_ELEMENTS(tts_btns); i++) {
        GtkWidget *b = gtk_button_new_with_label(tts_btns[i].lbl);
        gtk_style_context_add_class(gtk_widget_get_style_context(b), "btn-menu");
        g_signal_connect(b, "clicked", tts_btns[i].cb, app);
        gtk_box_pack_start(GTK_BOX(tts_bar), b, FALSE, FALSE, 0);
    }

    /* ── highlight toggle button ── */
    app->hl_btn = gtk_button_new_with_label("HL OFF");
    gtk_style_context_add_class(gtk_widget_get_style_context(app->hl_btn), "btn-menu");
    g_signal_connect(app->hl_btn, "clicked", G_CALLBACK(on_hl_toggle), app);
    gtk_box_pack_start(GTK_BOX(tts_bar), app->hl_btn, FALSE, FALSE, 0);

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
    app->paned = paned;
    gtk_paned_pack1(GTK_PANED(paned), app->notebook,   TRUE,  TRUE);
    gtk_paned_pack2(GTK_PANED(paned), app->tree_panel, FALSE, FALSE);
    gtk_paned_set_position(GTK_PANED(paned),
        app->_cfg_pane_pos > 0 ? app->_cfg_pane_pos : 600);
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

    /* ── status bar + RAM graph ───────────────────────────────────── */
    GtkWidget *bottom_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(outer), bottom_bar, FALSE, FALSE, 0);

    char *notes_dir = get_notes_dir();
    char *status_text = g_strdup_printf("dir: %s", notes_dir);
    app->status_label = gtk_label_new(status_text);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(app->status_label), "status-bar");
    gtk_widget_set_halign(app->status_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(bottom_bar), app->status_label, TRUE, TRUE, 0);
    g_free(status_text);
    g_free(notes_dir);

    /* RAM graph widget */
    app->ram_graph = gtk_drawing_area_new();
    gtk_widget_set_size_request(app->ram_graph, 160, 28);
    g_signal_connect(app->ram_graph, "draw",
        G_CALLBACK(on_ram_draw), app);
    gtk_box_pack_end(GTK_BOX(bottom_bar), app->ram_graph, FALSE, FALSE, 4);

    /* RAM label */
    GtkWidget *ram_lbl = gtk_label_new("RAM:");
    gtk_style_context_add_class(gtk_widget_get_style_context(ram_lbl), "status-bar");
    gtk_box_pack_end(GTK_BOX(bottom_bar), ram_lbl, FALSE, FALSE, 0);

    gtk_widget_show_all(app->window);

    /* kick off RAM sampling — first sample immediately, then every second */
    app->ram_head  = 0;
    app->ram_count = 0;
    on_ram_tick(app);
    app->ram_timer = g_timeout_add(1000, (GSourceFunc)on_ram_tick, app);

    /* load syntax highlighting config */
    hl_load_config(app);

    /* ── restore session ─────────────────────────────────────────── */
    /* notes dir: prefer conf value, fall back to default */
    char *start_dir = app->_cfg_notes_dir
        ? g_strdup(app->_cfg_notes_dir) : get_notes_dir();

    /* open last files, or a blank tab if none */
    if (app->_cfg_open_files && app->_cfg_open_files_n > 0) {
        for (int i = 0; i < app->_cfg_open_files_n; i++) {
            const char *f = app->_cfg_open_files[i];
            if (f && f[0] && g_file_test(f, G_FILE_TEST_EXISTS))
                tab_new(app, f);
        }
        /* if all files were missing, open a blank tab */
        if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook)) == 0)
            tab_new(app, NULL);
        /* restore active tab */
        gint n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook));
        gint active  = app->_cfg_active_tab;
        if (active >= 0 && active < n_pages)
            gtk_notebook_set_current_page(GTK_NOTEBOOK(app->notebook), active);
    } else {
        tab_new(app, NULL);
    }
    g_strfreev(app->_cfg_open_files);
    app->_cfg_open_files = NULL;

    /* populate tree */
    app->tree_monitor      = NULL;
    app->tree_refresh_timer = 0;
    tree_populate(app, start_dir);
    g_free(start_dir);
    g_free(app->_cfg_notes_dir);
    app->_cfg_notes_dir = NULL;

    /* restore tree visibility */
    if (!app->_cfg_tree_visible) {
        gtk_widget_hide(app->tree_panel);
        gtk_menu_item_set_label(
            GTK_MENU_ITEM(app->tree_toggle_item_widget), "SHOW TREE");
    }

    /* boot sequence fires after GTK main loop starts — deferred so session
     * restore messages (Loaded: ...) don't interleave with the animation */
    g_idle_add(boot_sequence_once, app);
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
    app->hl_enabled    = FALSE;

    app_load_config(app);
    build_ui(app);

    /* ── open file(s) passed on the command line (e.g. from file manager) ── */
    for (int i = 1; i < argc; i++) {
        if (argv[i] && argv[i][0] != '-' && g_file_test(argv[i], G_FILE_TEST_EXISTS))
            tab_new(app, argv[i]);
    }

    gtk_main();

    g_free(app);
    return 0;
}
