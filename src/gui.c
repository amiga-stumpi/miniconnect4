#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/dos.h>
#include <string.h>
#include "miniconnect4.h"

static struct IntuiText txt_new = { 0, 1, JAM1, 0, 1, 0, (UBYTE *)"New Game", 0 };
static struct IntuiText txt_quit = { 0, 1, JAM1, 0, 1, 0, (UBYTE *)"Quit", 0 };
static struct IntuiText txt_lobby = { 0, 1, JAM1, 0, 1, 0, (UBYTE *)"Lobby", 0 };
static struct IntuiText txt_disconnect = { 0, 1, JAM1, 0, 1, 0, (UBYTE *)"Disconnect", 0 };
static struct IntuiText txt_player_name = { 0, 1, JAM1, 0, 1, 0, (UBYTE *)"Player Name...", 0 };
static struct IntuiText txt_chat = { 0, 1, JAM1, 0, 1, 0, (UBYTE *)"Toggle Chat", 0 };
static struct IntuiText txt_sound_status = { 0, 1, JAM1, 0, 1, 0, (UBYTE *)"Sound Status", 0 };
static struct IntuiText txt_info = { 0, 1, JAM1, 0, 1, 0, (UBYTE *)"Info", 0 };

static struct MenuItem item_project_quit = { 0, 0, 10, 92, 10, ITEMTEXT | ITEMENABLED | HIGHCOMP, 0, (APTR)&txt_quit, 0, 0, 0, 0 };
static struct MenuItem item_project_new = { &item_project_quit, 0, 0, 92, 10, ITEMTEXT | ITEMENABLED | HIGHCOMP, 0, (APTR)&txt_new, 0, 0, 0, 0 };
static struct MenuItem item_network_disconnect = { 0, 0, 10, 112, 10, ITEMTEXT | ITEMENABLED | HIGHCOMP, 0, (APTR)&txt_disconnect, 0, 0, 0, 0 };
static struct MenuItem item_network_lobby = { &item_network_disconnect, 0, 0, 112, 10, ITEMTEXT | ITEMENABLED | HIGHCOMP, 0, (APTR)&txt_lobby, 0, 0, 0, 0 };
static struct MenuItem item_options_sound = { 0, 0, 20, 112, 10, ITEMTEXT | ITEMENABLED | HIGHCOMP, 0, (APTR)&txt_sound_status, 0, 0, 0, 0 };
static struct MenuItem item_options_chat = { &item_options_sound, 0, 10, 112, 10, ITEMTEXT | ITEMENABLED | HIGHCOMP, 0, (APTR)&txt_chat, 0, 0, 0, 0 };
static struct MenuItem item_options_name = { &item_options_chat, 0, 0, 112, 10, ITEMTEXT | ITEMENABLED | HIGHCOMP, 0, (APTR)&txt_player_name, 0, 0, 0, 0 };
static struct MenuItem item_help_info = { 0, 0, 0, 60, 10, ITEMTEXT | ITEMENABLED | HIGHCOMP, 0, (APTR)&txt_info, 0, 0, 0, 0 };

static struct Menu menu_help = { 0, 196, 0, 16, 10, MENUENABLED, (UBYTE *)"?", &item_help_info, 0, 0, 0, 0 };
static struct Menu menu_options = { &menu_help, 120, 0, 76, 10, MENUENABLED, (UBYTE *)"Options", &item_options_name, 0, 0, 0, 0 };
static struct Menu menu_network = { &menu_options, 54, 0, 66, 10, MENUENABLED, (UBYTE *)"Network", &item_network_lobby, 0, 0, 0, 0 };
static struct Menu menu_project = { &menu_network, 0, 0, 54, 10, MENUENABLED, (UBYTE *)"Project", &item_project_new, 0, 0, 0, 0 };

void gui_layout(struct MC4App *app)
{
    WORD ww = app->win->GZZWidth ? app->win->GZZWidth : app->win->Width;
    WORD wh = app->win->GZZHeight ? app->win->GZZHeight : app->win->Height;
    WORD top = 8;
    WORD min_cell = (wh < 240) ? 8 : 10;
    WORD chat_h = (WORD)((MC4_CHAT_LINES * 10) + 27);
    WORD avail_w = (WORD)(ww - 12);
    WORD avail_h;

    app->board_x = 6;
    app->board_y = top;
    app->chat_y = (WORD)(wh - chat_h);
    if (app->chat_y < top + 80)
        app->chat_y = (WORD)(top + 80);
    app->status_y = (WORD)(app->chat_y - 20);
    avail_h = (WORD)(app->status_y - app->board_y - 14);

    if (avail_w < MC4_COLS * min_cell)
        avail_w = MC4_COLS * min_cell;
    if (avail_h < MC4_ROWS * min_cell)
        avail_h = MC4_ROWS * min_cell;

    app->cell_w = (WORD)(avail_w / MC4_COLS);
    app->cell_h = (WORD)(avail_h / MC4_ROWS);
    if (app->cell_w < min_cell)
        app->cell_w = min_cell;
    if (app->cell_h < min_cell)
        app->cell_h = min_cell;
    app->cell = app->cell_w < app->cell_h ? app->cell_w : app->cell_h;
    app->board_w = (WORD)(app->cell_w * MC4_COLS);
    app->board_h = (WORD)(app->cell_h * MC4_ROWS);
}

void gui_enforce_aspect(struct MC4App *app)
{
    if (!app->win)
        return;
    app->last_win_w = app->win->Width;
    app->last_win_h = app->win->Height;
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
    if (h < 220) h = 220;

    for (i = 0; i < 3; ++i) {
        nw.LeftEdge = i == 0 ? x : 0;
        nw.TopEdge = i == 0 ? y : 0;
        nw.Width = i == 0 ? w : (i == 1 ? 320 : 300);
        nw.Height = i == 0 ? h : (i == 1 ? 240 : 220);
        nw.DetailPen = 0;
        nw.BlockPen = 1;
        nw.IDCMPFlags = IDCMP_CLOSEWINDOW | IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY | IDCMP_NEWSIZE | IDCMP_REFRESHWINDOW | IDCMP_MENUPICK;
        nw.Flags = WFLG_CLOSEGADGET | WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_SMART_REFRESH | WFLG_ACTIVATE | WFLG_GIMMEZEROZERO;
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
    SetMenuStrip(app->win, &menu_project);
    gui_layout(app);
    gui_draw_all(app);
    app->last_win_w = app->win->Width;
    app->last_win_h = app->win->Height;
    return 1;
}

void gui_close(struct MC4App *app)
{
    if (app->win) {
        ClearMenuStrip(app->win);
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

int gui_hit_column(struct MC4App *app, WORD x, WORD y)
{
    if (app->view != MC4_VIEW_GAME)
        return -1;
    if (x < app->board_x || x >= app->board_x + app->board_w)
        return -1;
    if (y < app->board_y || y >= app->board_y + app->board_h)
        return -1;
    return (x - app->board_x) / app->cell_w;
}

int gui_hit_lobby_player(struct MC4App *app, WORD x, WORD y)
{
    int idx;
    if (app->view != MC4_VIEW_LOBBY)
        return -1;
    if (x < app->board_x || x >= app->board_x + app->board_w)
        return -1;
    if (app->invite_pending)
        return -1;
    if (y < app->board_y + 21 || y >= app->board_y + app->board_h)
        return -1;
    idx = (y - (app->board_y + 21)) / 13;
    if (idx < 0 || idx >= app->lobby_count)
        return -1;
    if (app->lobby_players[idx].busy)
        return -1;
    return idx;
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

static void gui_text(struct RastPort *rp, WORD x, WORD y, const char *s)
{
    SetAPen(rp, 1);
    Move(rp, x, y);
    Text(rp, (STRPTR)s, util_len(s));
}

static void gui_box(struct RastPort *rp, WORD x1, WORD y1, WORD x2, WORD y2)
{
    SetAPen(rp, 1);
    Move(rp, x1, y1);
    Draw(rp, x2, y1);
    Draw(rp, x2, y2);
    Draw(rp, x1, y2);
    Draw(rp, x1, y1);
}

void gui_edit_player_name(struct MC4App *app)
{
    struct NewWindow nw;
    struct Window *w;
    struct IntuiMessage *msg;
    struct StringInfo si;
    struct Gadget name_gad;
    struct Gadget ok_gad;
    struct Gadget cancel_gad;
    struct IntuiText ok_text;
    struct IntuiText cancel_text;
    char buf[MC4_NAME_LEN + 1];
    char undo[MC4_NAME_LEN + 1];
    ULONG classv;
    UWORD code;
    int done = 0;
    int accept = 0;

    util_copy(buf, sizeof(buf), app->cfg.player_name);
    undo[0] = 0;
    memset(&si, 0, sizeof(si));
    si.Buffer = (STRPTR)buf;
    si.UndoBuffer = (STRPTR)undo;
    si.MaxChars = sizeof(buf);

    ok_text.FrontPen = 1; ok_text.BackPen = 0; ok_text.DrawMode = JAM1;
    ok_text.LeftEdge = 12; ok_text.TopEdge = 4; ok_text.ITextFont = 0;
    ok_text.IText = (UBYTE *)"OK"; ok_text.NextText = 0;
    cancel_text.FrontPen = 1; cancel_text.BackPen = 0; cancel_text.DrawMode = JAM1;
    cancel_text.LeftEdge = 8; cancel_text.TopEdge = 4; cancel_text.ITextFont = 0;
    cancel_text.IText = (UBYTE *)"Cancel"; cancel_text.NextText = 0;

    memset(&name_gad, 0, sizeof(name_gad));
    memset(&ok_gad, 0, sizeof(ok_gad));
    memset(&cancel_gad, 0, sizeof(cancel_gad));

    name_gad.NextGadget = &ok_gad;
    name_gad.LeftEdge = 16; name_gad.TopEdge = 32; name_gad.Width = 208; name_gad.Height = 12;
    name_gad.Flags = GFLG_GADGHCOMP;
    name_gad.Activation = GACT_RELVERIFY | GACT_STRINGLEFT;
    name_gad.GadgetType = GTYP_STRGADGET;
    name_gad.SpecialInfo = (APTR)&si;
    name_gad.GadgetID = 1;

    ok_gad.NextGadget = &cancel_gad;
    ok_gad.LeftEdge = 42; ok_gad.TopEdge = 58; ok_gad.Width = 52; ok_gad.Height = 16;
    ok_gad.Flags = GFLG_GADGHCOMP;
    ok_gad.Activation = GACT_RELVERIFY;
    ok_gad.GadgetType = GTYP_BOOLGADGET;
    ok_gad.GadgetText = &ok_text;
    ok_gad.GadgetID = 2;

    cancel_gad.LeftEdge = 122; cancel_gad.TopEdge = 58; cancel_gad.Width = 76; cancel_gad.Height = 16;
    cancel_gad.Flags = GFLG_GADGHCOMP;
    cancel_gad.Activation = GACT_RELVERIFY;
    cancel_gad.GadgetType = GTYP_BOOLGADGET;
    cancel_gad.GadgetText = &cancel_text;
    cancel_gad.GadgetID = 3;

    memset(&nw, 0, sizeof(nw));
    nw.LeftEdge = app->win ? (WORD)(app->win->LeftEdge + 24) : 24;
    nw.TopEdge = app->win ? (WORD)(app->win->TopEdge + 24) : 24;
    nw.Width = 240;
    nw.Height = 88;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = IDCMP_CLOSEWINDOW | IDCMP_GADGETUP | IDCMP_RAWKEY;
    nw.Flags = WFLG_CLOSEGADGET | WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_SMART_REFRESH | WFLG_ACTIVATE | WFLG_GIMMEZEROZERO;
    nw.FirstGadget = &name_gad;
    nw.CheckMark = 0;
    nw.Title = (UBYTE *)"Player Name";
    nw.Screen = 0;
    nw.BitMap = 0;
    nw.MinWidth = 240;
    nw.MinHeight = 88;
    nw.MaxWidth = 240;
    nw.MaxHeight = 88;
    nw.Type = WBENCHSCREEN;

    w = OpenWindow(&nw);
    if (!w) {
        gui_set_status(app, "Name window failed");
        return;
    }

    SetAPen(w->RPort, 0);
    RectFill(w->RPort, 0, 0, w->Width - 1, w->Height - 1);
    gui_text(w->RPort, 16, 24, "Name:");
    gui_box(w->RPort, 14, 30, 226, 46);
    gui_box(w->RPort, 42, 58, 94, 74);
    gui_box(w->RPort, 122, 58, 198, 74);
    RefreshGList(&name_gad, w, 0, 3);
    ActivateGadget(&name_gad, w, 0);

    while (!done) {
        Wait(1L << w->UserPort->mp_SigBit);
        while ((msg = (struct IntuiMessage *)GetMsg(w->UserPort))) {
            classv = msg->Class;
            code = msg->Code;
            if (classv == IDCMP_GADGETUP && msg->IAddress) {
                struct Gadget *g = (struct Gadget *)msg->IAddress;
                if (g->GadgetID == 2) {
                    accept = 1;
                    done = 1;
                } else if (g->GadgetID == 3) {
                    done = 1;
                }
            } else if (classv == IDCMP_CLOSEWINDOW) {
                done = 1;
            } else if (classv == IDCMP_RAWKEY && code == 0x44) {
                accept = 1;
                done = 1;
            }
            ReplyMsg((struct Message *)msg);
        }
    }

    if (accept) {
        if (!buf[0])
            config_make_default_name(buf, sizeof(buf));
        util_copy(app->cfg.player_name, sizeof(app->cfg.player_name), buf);
        config_save(&app->cfg);
        gui_set_status(app, "Player name saved");
    }
    CloseWindow(w);
}

int gui_confirm_invite(struct MC4App *app, const char *name)
{
    struct IntuiText body;
    struct IntuiText yes;
    struct IntuiText no;
    char text[96];

    util_copy(text, sizeof(text), "Anfrage von ");
    util_append(text, sizeof(text), name);
    util_append(text, sizeof(text), " annehmen?");

    body.FrontPen = 1; body.BackPen = 0; body.DrawMode = JAM1;
    body.LeftEdge = 8; body.TopEdge = 8; body.ITextFont = 0;
    body.IText = (UBYTE *)text; body.NextText = 0;

    yes.FrontPen = 1; yes.BackPen = 0; yes.DrawMode = JAM1;
    yes.LeftEdge = 6; yes.TopEdge = 3; yes.ITextFont = 0;
    yes.IText = (UBYTE *)"Annehmen"; yes.NextText = 0;

    no.FrontPen = 1; no.BackPen = 0; no.DrawMode = JAM1;
    no.LeftEdge = 6; no.TopEdge = 3; no.ITextFont = 0;
    no.IText = (UBYTE *)"Ablehnen"; no.NextText = 0;

    return AutoRequest(app->win, &body, &yes, &no, 0, 0, 260, 70) ? 1 : 0;
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
