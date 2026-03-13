#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <cairo.h>
#include <math.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ── Config ───────────────────────────────────────── */
#define WIN_W        420   // widened from 320
#define WIN_H        300
#define DROP_COLS    24
#define DROP_MAX_LEN 18
#define FPS          30
#define RAM_HISTORY  120

/* Matrix chars */
static const char *MATRIX_CHARS =
"ｱｲｳｴｵｶｷｸｹｺｻｼｽｾｿﾀﾁﾂﾃﾄﾅﾆﾇﾈﾉﾊﾋﾌﾍﾎﾏﾐﾑﾒﾓﾔﾕﾖﾗﾘﾙﾚﾛﾜｦﾝ"
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*<>?/|\\~`";

/* ── Matrix drop state ────────────────────────────── */
typedef struct {
    double x;
    double y;
    double speed;
    int len;
    char chars[DROP_MAX_LEN][8];
    double alpha[DROP_MAX_LEN];
} Drop;

static Drop drops[DROP_COLS];
static int n_drops = 0;

/* ── RAM graph state ──────────────────────────────── */
static float ram_hist[RAM_HISTORY];
static int ram_index = 0;

/* ── Widget ───────────────────────────────────────── */
static GtkWidget *canvas;
static int drag_x, drag_y;
static gboolean dragging = FALSE;

/* ── 12/24 hour format ───────────────────────────── */
static gboolean use_24_hour = TRUE;  // start in 24h

/* ── Color theme (0=green, 1=amber) ─────────────── */
/* Cycled with middle mouse button — no switch needed */
static int color_theme = 0;

/* theme base colors [theme] */
static const double THEME_R[] = { 0.0,  0.91 };
static const double THEME_G[] = { 1.0,  0.63 };
static const double THEME_B[] = { 0.6,  0.13 };

/* ── Random helpers ───────────────────────────────── */
static double randf(void){ return (double)rand()/RAND_MAX; }

static void rand_char(char *out){
    int total=0;
    const char *p=MATRIX_CHARS;
    while(*p){ total++; p+=(*p&0x80)?((*p&0xE0)==0xC0?2:3):1; }

    int pick=rand()%total;
    p=MATRIX_CHARS;

    for(int i=0;i<pick;i++)
        p+=(*p&0x80)?((*p&0xE0)==0xC0?2:3):1;

    int bytes=(*p&0x80)?((*p&0xE0)==0xC0?2:3):1;

    memcpy(out,p,bytes);
    out[bytes]='\0';
}

/* ── Matrix init ──────────────────────────────────── */
static void init_drop(Drop *d,int col){
    d->x=(col*(WIN_W/DROP_COLS))+(WIN_W/DROP_COLS/2.0);
    d->y=-(randf()*WIN_H);
    d->speed=2.0+randf()*3.5;
    d->len=5+rand()%(DROP_MAX_LEN-5);

    for(int i=0;i<d->len;i++){
        rand_char(d->chars[i]);
        d->alpha[i]=(double)(d->len-i)/d->len;
    }
}

static void init_drops(void){
    n_drops=DROP_COLS;
    for(int i=0;i<n_drops;i++)
        init_drop(&drops[i],i);
}

static void update_drops(void){
    int cell_h=13;

    for(int i=0;i<n_drops;i++){
        drops[i].y+=drops[i].speed;

        if(rand()%3==0){
            int idx=rand()%drops[i].len;
            rand_char(drops[i].chars[idx]);
        }

        if(drops[i].y-drops[i].len*cell_h>WIN_H)
            init_drop(&drops[i],i);
    }
}

/* ── RAM usage reader ─────────────────────────────── */
static float get_ram_usage(){

    FILE *f=fopen("/proc/meminfo","r");
    if(!f) return 0;

    long total=0;
    long avail=0;

    char key[64];
    long val;
    char unit[16];

    while(fscanf(f,"%63s %ld %15s\n",key,&val,unit)==3){
        if(strcmp(key,"MemTotal:")==0) total=val;
        if(strcmp(key,"MemAvailable:")==0) avail=val;
    }

    fclose(f);

    if(total==0) return 0;

    return 100.0f*(1.0f-((float)avail/(float)total));
}

/* ── System info ──────────────────────────────────── */
static void get_uptime(char *out,int size){

    FILE *f=fopen("/proc/uptime","r");

    if(!f){
        snprintf(out,size,"uptime: unknown");
        return;
    }

    double up;
    fscanf(f,"%lf",&up);
    fclose(f);

    int d=(int)(up/86400);
    int h=(int)(up/3600)%24;
    int m=(int)(up/60)%60;
    int s=(int)up%60;

    if(d>0)
        snprintf(out,size,"up %dd %02dh %02dm %02ds",d,h,m,s);
    else
        snprintf(out,size,"up %02dh %02dm %02ds",h,m,s);
}

/* ── RAM graph draw ───────────────────────────────── */
static void draw_ram_graph(cairo_t *cr,double px,double py,double pw){

    /* panel bottom is at py-14+ph = py-14+(WIN_H-100)
     * sit the graph baseline 3px above the panel border so it's flush */
    double panel_bottom = py - 14 + (WIN_H - 100);
    double gy           = panel_bottom - 3;
    int    graph_h      = 16;

    cairo_set_source_rgba(cr,THEME_R[color_theme],THEME_G[color_theme],THEME_B[color_theme],0.85);
    cairo_set_line_width(cr,1.4);

    for(int i=0;i<RAM_HISTORY;i++){

        int idx=(ram_index+i)%RAM_HISTORY;
        float val=ram_hist[idx];

        double x=px+(double)i/RAM_HISTORY*pw;
        double y=gy+graph_h-(val/100.0)*graph_h;

        if(i==0)
            cairo_move_to(cr,x,y);
        else
            cairo_line_to(cr,x,y);
    }

    cairo_stroke(cr);

    /* ── RAM label: show used RAM as e.g. "1.2 GB" or "820 MB" ── */
    FILE *f = fopen("/proc/meminfo","r");
    if(f){
        long total_kb=0, avail_kb=0;
        char key[64]; long v; char unit[16];
        while(fscanf(f,"%63s %ld %15s\n",key,&v,unit)==3){
            if(strcmp(key,"MemTotal:")==0)     total_kb=v;
            if(strcmp(key,"MemAvailable:")==0) avail_kb=v;
        }
        fclose(f);

        long used_kb = total_kb - avail_kb;
        char ram_label[32];

        if(used_kb >= 1024*1024)
            snprintf(ram_label,sizeof(ram_label),"%.1f GB",used_kb/1024.0/1024.0);
        else
            snprintf(ram_label,sizeof(ram_label),"%ld MB",used_kb/1024);

        cairo_select_font_face(cr,"Monospace",CAIRO_FONT_SLANT_NORMAL,CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr,13.0);
        cairo_text_extents_t te;
        cairo_text_extents(cr,ram_label,&te);

        /* right-align the label inside the panel, baseline on gy */
        double lx = px + pw - te.width - te.x_bearing - 2;
        double ly = gy - 2;

        cairo_set_source_rgba(cr,
            THEME_R[color_theme],
            THEME_G[color_theme]*0.8,
            THEME_B[color_theme]*0.7, 0.75);
        cairo_move_to(cr, lx, ly);
        cairo_show_text(cr, ram_label);
    }
}

/* ── Draw ─────────────────────────────────────────── */
static gboolean on_draw(GtkWidget *w,cairo_t *cr,gpointer data){

    int cell_h=13;

    cairo_set_source_rgba(cr,0.0,0.0,0.05,0.85);
    cairo_paint(cr);

    cairo_select_font_face(cr,"Monospace",CAIRO_FONT_SLANT_NORMAL,CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr,11.0);

    /* Matrix rain */
    for(int i=0;i<n_drops;i++){

        Drop *d=&drops[i];

        for(int j=0;j<d->len;j++){

            double cy=d->y-j*cell_h;

            if(cy<-cell_h||cy>WIN_H+cell_h) continue;

            double alpha=d->alpha[j]*0.55;

            if(j==0)
                cairo_set_source_rgba(cr,
                    THEME_R[color_theme]*0.9+0.1,
                    THEME_G[color_theme]*0.9+0.1,
                    THEME_B[color_theme]*0.9+0.1, 0.9);
            else
                cairo_set_source_rgba(cr,
                    THEME_R[color_theme],
                    THEME_G[color_theme]*(0.8-j*0.03),
                    THEME_B[color_theme], alpha);

            cairo_move_to(cr,d->x-5,cy);
            cairo_show_text(cr,d->chars[j]);
        }
    }

    /* Panel */
    double px=18, py=60, pw=WIN_W-36, ph=WIN_H-100;

    cairo_set_source_rgba(cr,0.0,0.02,0.06,0.78);
    cairo_rectangle(cr,px,py-14,pw,ph);
    cairo_fill(cr);

    cairo_set_source_rgba(cr,THEME_R[color_theme],THEME_G[color_theme],THEME_B[color_theme],0.35);
    cairo_set_line_width(cr,1.0);
    cairo_rectangle(cr,px,py-14,pw,ph);
    cairo_stroke(cr);

    time_t now=time(NULL);
    struct tm *t=localtime(&now);

    char time_str[16],date_str[32],host_str[64],up_str[64];

    if(use_24_hour)
        strftime(time_str,sizeof(time_str),"%H:%M:%S",t);
    else
        strftime(time_str,sizeof(time_str),"%I:%M:%S %p",t);

    strftime(date_str,sizeof(date_str),"%A  %Y.%m.%d",t);
    gethostname(host_str,sizeof(host_str));
    get_uptime(up_str,sizeof(up_str));

    cairo_text_extents_t te;

    // ── Time ──
    cairo_select_font_face(cr,"Monospace",CAIRO_FONT_SLANT_NORMAL,CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr,60.0);

    cairo_text_extents(cr,time_str,&te);
    double tx=px+(pw-te.width)/2.0-te.x_bearing;

    cairo_set_source_rgba(cr,THEME_R[color_theme],THEME_G[color_theme],THEME_B[color_theme],1.0);
    cairo_move_to(cr,tx,py);
    cairo_show_text(cr,time_str);

    // ── Date ──
    cairo_select_font_face(cr,"Monospace",CAIRO_FONT_SLANT_NORMAL,CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr,16.0);

    cairo_text_extents(cr,date_str,&te);
    cairo_set_source_rgba(cr,THEME_R[color_theme],THEME_G[color_theme]*0.9,THEME_B[color_theme]*0.8,0.9);
    cairo_move_to(cr,px+(pw-te.width)/2.0-te.x_bearing,py+50);
    cairo_show_text(cr,date_str);

    // ── Host ──
    char host_label[80];
    snprintf(host_label,sizeof(host_label),"▸ %s",host_str);

    cairo_set_font_size(cr,14.0);
    cairo_text_extents(cr,host_label,&te);
    cairo_set_source_rgba(cr,THEME_R[color_theme],THEME_G[color_theme]*0.7,THEME_B[color_theme]*0.6,0.75);
    cairo_move_to(cr,px+(pw-te.width)/2.0-te.x_bearing,py+90);
    cairo_show_text(cr,host_label);

    // ── Uptime ──
    char up_label[80];
    snprintf(up_label,sizeof(up_label),"▸ %s",up_str);

    cairo_text_extents(cr,up_label,&te);
    cairo_set_source_rgba(cr,THEME_R[color_theme],THEME_G[color_theme]*0.6,THEME_B[color_theme]*0.5,0.65);
    cairo_move_to(cr,px+(pw-te.width)/2.0-te.x_bearing,py+120);
    cairo_show_text(cr,up_label);

    /* RAM line graph */
    draw_ram_graph(cr,px,py,pw);

    return FALSE;
}

/* ── Tick ─────────────────────────────────────────── */
static gboolean on_tick(gpointer data){

    update_drops();

    ram_hist[ram_index]=get_ram_usage();
    ram_index=(ram_index+1)%RAM_HISTORY;

    gtk_widget_queue_draw(canvas);

    return G_SOURCE_CONTINUE;
}

/* ── Drag + Click handlers ───────────────────────── */
static gboolean on_button_press(GtkWidget *w,GdkEventButton *e,gpointer d){

    if(e->button == 1){
        if(e->type == GDK_2BUTTON_PRESS){
            /* double left click — toggle 12/24h, don't start drag */
            use_24_hour = !use_24_hour;
            gtk_widget_queue_draw(canvas);
        } else if(e->type == GDK_BUTTON_PRESS){
            /* single left click — start drag only */
            drag_x = e->x_root;
            drag_y = e->y_root;
            dragging = TRUE;
        }
    }

    if(e->button == 2){
        /* middle click — cycle color theme (green → amber → green) */
        color_theme = (color_theme + 1) % 2;
        gtk_widget_queue_draw(canvas);
    }

    if(e->button == 3){
        /* right click — close */
        gtk_main_quit();
    }

    return TRUE;
}

static gboolean on_button_release(GtkWidget *w,GdkEventButton *e,gpointer d){
    dragging=FALSE;
    return TRUE;
}

static gboolean on_motion(GtkWidget *w,GdkEventMotion *e,gpointer d){

    if(!dragging) return TRUE;

    GtkWindow *win=GTK_WINDOW(gtk_widget_get_toplevel(w));

    int wx,wy;
    gtk_window_get_position(win,&wx,&wy);

    int dx=e->x_root-drag_x;
    int dy=e->y_root-drag_y;

    gtk_window_move(win,wx+dx,wy+dy);

    drag_x=e->x_root;
    drag_y=e->y_root;

    return TRUE;
}

/* ── Main ─────────────────────────────────────────── */
int main(int argc,char *argv[]){

    srand(time(NULL));
    gtk_init(&argc,&argv);
    init_drops();

    GtkWidget *win=gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_window_set_title(GTK_WINDOW(win),"cyon_clock");
    gtk_window_set_default_size(GTK_WINDOW(win),WIN_W,WIN_H);
    gtk_window_set_decorated(GTK_WINDOW(win),FALSE);
    gtk_window_set_keep_below(GTK_WINDOW(win),TRUE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(win),TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(win),TRUE);
    gtk_window_stick(GTK_WINDOW(win));

    GdkScreen *screen=gtk_widget_get_screen(win);
    GdkVisual *visual=gdk_screen_get_rgba_visual(screen);

    if(visual) gtk_widget_set_visual(win,visual);

    gtk_widget_set_app_paintable(win,TRUE);

    canvas=gtk_drawing_area_new();
    gtk_widget_set_size_request(canvas,WIN_W,WIN_H);

    gtk_widget_add_events(canvas,
        GDK_BUTTON_PRESS_MASK|
        GDK_BUTTON_RELEASE_MASK|
        GDK_POINTER_MOTION_MASK);

    g_signal_connect(canvas,"draw",G_CALLBACK(on_draw),NULL);
    g_signal_connect(canvas,"button-press-event",G_CALLBACK(on_button_press),NULL);
    g_signal_connect(canvas,"button-release-event",G_CALLBACK(on_button_release),NULL);
    g_signal_connect(canvas,"motion-notify-event",G_CALLBACK(on_motion),NULL);
    g_signal_connect(win,"destroy",G_CALLBACK(gtk_main_quit),NULL);

    gtk_container_add(GTK_CONTAINER(win),canvas);

    gtk_widget_show_all(win);

    g_timeout_add(1000/FPS,on_tick,NULL);

    gtk_main();
    return 0;
}