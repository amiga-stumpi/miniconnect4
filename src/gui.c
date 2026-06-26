#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/dos.h>
#include "miniconnect4.h"

#define BTN_NEW 1
#define BTN_HOST 2
#define BTN_JOIN 3
#define BTN_DISC 4
#define BTN_CHAT 5
#define BTN_INFO 6
#define BTN_QUIT 7


static void set_button(struct MC4Button *b, WORD x, WORD y, WORD w, WORD h, UWORD id, const char *label)
{
    b->x = x; b->y = y; b->w = w; b->h = h; b->id = id; b->label = label;
}

void gui_layout(struct MC4App *app)
{
    WORD ww = app->win->Width;
    WORD wh = app->win->Height;
    WORD top = 22;
    WORD side = 100;
    WORD gap = 8;
    WORD avail_w = (WORD)(ww - side - gap - 16);
    WORD avail_h = (WORD)(wh - top - (app->cfg.chat_enabled ? 72 : 32));
    WORD cell_w = (WORD)(avail_w / MC4_COLS);
    WORD cell_h = (WORD)(avail_h / MC4_ROWS);
    WORD by;
    WORD bx;
    UWORD n = 0;

    app->cell = cell_w < cell_h ? cell_w : cell_h;
    if (app->cell < 18)
        app->cell = 18;
    app->board_w = (WORD)(app->cell * MC4_COLS);
    app->board_h = (WORD)(app->cell * MC4_ROWS);
    app->board_x = 8;
    app->board_y = top;
    app->status_y = (WORD)(app->board_y + app->board_h + 16);
    app->chat_y = (WORD)(app->status_y + 16);

    bx = (WORD)(app->board_x + app->board_w + gap);
    if (bx + 88 > ww - 4)
        bx = (WORD)(ww - 92);
    if (bx < app->board_x + app->board_w + 2)
        bx = (WORD)(app->board_x + app->board_w + 2);

    by = top;
    set_button(&app->buttons[n++], bx, by, 88, 16, BTN_NEW, "New"); by += 20;
    set_button(&app->buttons[n++], bx, by, 88, 16, BTN_HOST, "Host"); by += 20;
    set_button(&app->buttons[n++], bx, by, 88, 16, BTN_JOIN, "Join"); by += 20;
    set_button(&app->buttons[n++], bx, by, 88, 16, BTN_DISC, "Disconnect"); by += 20;
    set_button(&app->buttons[n++], bx, by, 88, 16, BTN_CHAT, app->cfg.chat_enabled ? "Chat On" : "Chat Off"); by += 20;
    set_button(&app->buttons[n++], bx, by, 88, 16, BTN_INFO, "Info"); by += 20;
    set_button(&app->buttons[n++], bx, by, 88, 16, BTN_QUIT, "Quit");
    app->button_count = n;
}

static WORD abs_word(WORD v)
{
    return v < 0 ? (WORD)-v : v;
}

void gui_enforce_aspect(struct MC4App *app)
{
    WORD w;
    WORD h;
    WORD dw;
    WORD dh;
    WORD target_w;
    WORD target_h;
    WORD max_w;
    WORD max_h;

    if (!app->win)
        return;

    w = app->win->Width;
    h = app->win->Height;
    if (app->last_win_w == 0 || app->last_win_h == 0) {
        app->last_win_w = w;
        app->last_win_h = h;
        return;
    }

    if (app->resize_lock) {
        app->resize_lock = 0;
        app->last_win_w = w;
        app->last_win_h = h;
        return;
    }

    dw = abs_word((WORD)(w - app->last_win_w));
    dh = abs_word((WORD)(h - app->last_win_h));
    target_w = w;
    target_h = h;

    if (dw >= dh)
        target_h = (WORD)(((LONG)w * 220L + 195L) / 390L);
    else
        target_w = (WORD)(((LONG)h * 390L + 110L) / 220L);

    if (target_w < 300)
        target_w = 300;
    if (target_h < 180)
        target_h = 180;

    max_w = 640;
    max_h = 256;
    if (app->screen) {
        max_w = (WORD)(app->screen->Width - app->win->LeftEdge - 2);
        max_h = (WORD)(app->screen->Height - app->win->TopEdge - 2);
    }
    if (max_w < 300)
        max_w = 300;
    if (max_h < 180)
        max_h = 180;

    if (target_h > max_h) {
        target_h = max_h;
        target_w = (WORD)(((LONG)target_h * 390L + 110L) / 220L);
    }
    if (target_w > max_w) {
        target_w = max_w;
        target_h = (WORD)(((LONG)target_w * 220L + 195L) / 390L);
    }
    if (target_w < 300)
        target_w = 300;
    if (target_h < 180)
        target_h = 180;

    if (target_w != w || target_h != h) {
        app->resize_lock = 1;
        SizeWindow(app->win, (WORD)(target_w - w), (WORD)(target_h - h));
        app->last_win_w = target_w;
        app->last_win_h = target_h;
    } else {
        app->last_win_w = w;
        app->last_win_h = h;
    }
}

int gui_open(struct MC4App *app)
{
    struct NewWindow nw;
    WORD x, y, w, h;
    int i;

    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 0);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 0);
    if (!IntuitionBase || !GfxBase)
        return 0;

    x = app->cfg.win_x; y = app->cfg.win_y; w = app->cfg.win_w; h = app->cfg.win_h;
    if (w < 300) w = 300;
    if (h < 180) h = 180;

    for (i = 0; i < 3; ++i) {
        nw.LeftEdge = i == 0 ? x : 0;
        nw.TopEdge = i == 0 ? y : 0;
        nw.Width = i == 0 ? w : (i == 1 ? 320 : 300);
        nw.Height = i == 0 ? h : (i == 1 ? 200 : 180);
        nw.DetailPen = 0;
        nw.BlockPen = 1;
        nw.IDCMPFlags = IDCMP_CLOSEWINDOW | IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY | IDCMP_NEWSIZE | IDCMP_REFRESHWINDOW;
        nw.Flags = WFLG_CLOSEGADGET | WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_SMART_REFRESH | WFLG_ACTIVATE;
        if (i < 2)
            nw.Flags |= WFLG_SIZEGADGET;
        nw.FirstGadget = 0;
        nw.CheckMark = 0;
        nw.Title = (UBYTE *)MC4_NAME;
        nw.Screen = 0;
        nw.BitMap = 0;
        nw.MinWidth = 300;
        nw.MinHeight = 180;
        nw.MaxWidth = -1;
        nw.MaxHeight = -1;
        nw.Type = WBENCHSCREEN;
        app->win = OpenWindow(&nw);
        if (app->win)
            break;
    }
    if (!app->win)
        return 0;
    app->rp = app->win->RPort;
    app->screen = app->win->WScreen;
    gui_layout(app);
    gui_draw_all(app);
    app->last_win_w = app->win->Width;
    app->last_win_h = app->win->Height;
    return 1;
}

void gui_close(struct MC4App *app)
{
    if (app->win) {
        app->cfg.win_x = app->win->LeftEdge;
        app->cfg.win_y = app->win->TopEdge;
        app->cfg.win_w = app->win->Width;
        app->cfg.win_h = app->win->Height;
        CloseWindow(app->win);
        app->win = 0;
    }
    if (GfxBase) { CloseLibrary((struct Library *)GfxBase); GfxBase = 0; }
    if (IntuitionBase) { CloseLibrary((struct Library *)IntuitionBase); IntuitionBase = 0; }
}

int gui_hit_button(struct MC4App *app, WORD x, WORD y)
{
    UWORD i;
    struct MC4Button *b;
    for (i = 0; i < app->button_count; ++i) {
        b = &app->buttons[i];
        if (x >= b->x && x <= b->x + b->w && y >= b->y && y <= b->y + b->h)
            return b->id;
    }
    return 0;
}

int gui_hit_column(struct MC4App *app, WORD x, WORD y)
{
    if (x < app->board_x || x >= app->board_x + app->board_w)
        return -1;
    if (y < app->board_y || y >= app->board_y + app->board_h)
        return -1;
    return (x - app->board_x) / app->cell;
}

void gui_set_status(struct MC4App *app, const char *s)
{
    util_copy(app->status, sizeof(app->status), s);
    if (app->win)
        gui_draw_status(app);
}

void gui_add_chat(struct MC4App *app, const char *s)
{
    int i;
    if (!app->cfg.chat_enabled)
        return;
    if (app->chat_count < MC4_CHAT_LINES) {
        util_copy(app->chat_lines[app->chat_count++], MC4_CHAT_LEN, s);
    } else {
        for (i = 1; i < MC4_CHAT_LINES; ++i)
            util_copy(app->chat_lines[i - 1], MC4_CHAT_LEN, app->chat_lines[i]);
        util_copy(app->chat_lines[MC4_CHAT_LINES - 1], MC4_CHAT_LEN, s);
    }
    gui_draw_chat(app);
}

void gui_info(struct MC4App *app)
{
    struct IntuiText body;
    struct IntuiText ok;

    body.FrontPen = 1;
    body.BackPen = 0;
    body.DrawMode = JAM1;
    body.LeftEdge = 8;
    body.TopEdge = 8;
    body.ITextFont = 0;
    body.IText = (UBYTE *)"MiniConnect4\nOnline Connect Four for AmigaOS 1.3\nMarcel Jaehne (c)2026";
    body.NextText = 0;

    ok.FrontPen = 1;
    ok.BackPen = 0;
    ok.DrawMode = JAM1;
    ok.LeftEdge = 6;
    ok.TopEdge = 3;
    ok.ITextFont = 0;
    ok.IText = (UBYTE *)"OK";
    ok.NextText = 0;

    AutoRequest(app->win, &body, 0, &ok, 0, 0, 300, 90);
}
