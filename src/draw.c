#include <proto/graphics.h>
#include <proto/dos.h>
#include "miniconnect4.h"

static void draw_text(struct RastPort *rp, WORD x, WORD y, const char *s, UBYTE pen)
{
    SetAPen(rp, pen);
    Move(rp, x, y);
    Text(rp, (STRPTR)s, util_len(s));
}

static void fill_rect(struct RastPort *rp, WORD x1, WORD y1, WORD x2, WORD y2, UBYTE pen)
{
    SetAPen(rp, pen);
    RectFill(rp, x1, y1, x2, y2);
}

static void frame_rect(struct RastPort *rp, WORD x1, WORD y1, WORD x2, WORD y2, UBYTE pen)
{
    SetAPen(rp, pen);
    Move(rp, x1, y1); Draw(rp, x2, y1); Draw(rp, x2, y2); Draw(rp, x1, y2); Draw(rp, x1, y1);
}

static void draw_disc(struct MC4App *app, WORD cx, WORD cy, WORD r, UBYTE value)
{
    UBYTE pen;
    WORD x, y;
    WORD rr;
    struct RastPort *rp = app->rp;

    if (value == MC4_P1)
        pen = app->cfg.pen_p1;
    else if (value == MC4_P2)
        pen = app->cfg.pen_p2;
    else
        pen = app->cfg.pen_bg;

    rr = r * r;
    SetAPen(rp, pen);
    for (y = -r; y <= r; ++y) {
        for (x = -r; x <= r; ++x) {
            if ((x * x) + (y * y) <= rr)
                WritePixel(rp, cx + x, cy + y);
        }
    }
    SetAPen(rp, app->cfg.pen_board);
    for (x = -r; x <= r; ++x) {
        WritePixel(rp, cx + x, cy - r);
        WritePixel(rp, cx + x, cy + r);
    }
}

void gui_draw_cell(struct MC4App *app, int row, int col, UBYTE value)
{
    WORD x = app->board_x + (WORD)(col * app->cell_w);
    WORD y = app->board_y + (WORD)(row * app->cell_h);
    WORD r = (WORD)(((app->cell_w < app->cell_h ? app->cell_w : app->cell_h) / 2) - 3);

    fill_rect(app->rp, x + 1, y + 1, x + app->cell_w - 2, y + app->cell_h - 2, app->cfg.pen_board);
    draw_disc(app, (WORD)(x + app->cell_w / 2), (WORD)(y + app->cell_h / 2), r, value);
}

static void draw_board(struct MC4App *app)
{
    int r, c;
    fill_rect(app->rp, app->board_x, app->board_y,
              app->board_x + app->board_w, app->board_y + app->board_h, app->cfg.pen_board);
    for (r = 0; r < MC4_ROWS; ++r)
        for (c = 0; c < MC4_COLS; ++c)
            gui_draw_cell(app, r, c, app->game.board[r][c]);
    frame_rect(app->rp, app->board_x, app->board_y,
               app->board_x + app->board_w, app->board_y + app->board_h, app->cfg.pen_text);
}

void gui_draw_lobby(struct MC4App *app)
{
    int i;
    WORD y;
    char line[80];
    WORD ww = app->win->GZZWidth ? app->win->GZZWidth : app->win->Width;

    fill_rect(app->rp, app->board_x, app->board_y,
              app->board_x + app->board_w, app->board_y + app->board_h, app->cfg.pen_bg);
    frame_rect(app->rp, app->board_x, app->board_y,
               app->board_x + app->board_w, app->board_y + app->board_h, app->cfg.pen_text);
    draw_text(app->rp, app->board_x + 8, app->board_y + 14, tr(&app->cfg, MC4_TX_LOBBY_PLAYERS), app->cfg.pen_text);
    y = (WORD)(app->board_y + 30);
    if (app->lobby_count == 0) {
        draw_text(app->rp, app->board_x + 8, y, tr(&app->cfg, MC4_TX_NO_PLAYERS), app->cfg.pen_text);
        return;
    }
    for (i = 0; i < app->lobby_count; ++i) {
        if (y > app->board_y + app->board_h - 10)
            break;
        util_copy(line, sizeof(line), app->lobby_players[i].name);
        util_append(line, sizeof(line), app->lobby_players[i].busy ? tr(&app->cfg, MC4_TX_IN_GAME) : tr(&app->cfg, MC4_TX_REQUEST_GAME));
        if (!app->lobby_players[i].busy)
            fill_rect(app->rp, app->board_x + 4, y - 9, ww - 12, y + 3, app->cfg.pen_board);
        draw_text(app->rp, app->board_x + 8, y, line, app->cfg.pen_text);
        y += 13;
    }
}


void gui_draw_status(struct MC4App *app)
{
    WORD ww = app->win->GZZWidth ? app->win->GZZWidth : app->win->Width;
    fill_rect(app->rp, 4, app->status_y - 10, ww - 8, app->status_y + 3, app->cfg.pen_bg);
    frame_rect(app->rp, 4, app->status_y - 11, ww - 8, app->status_y + 4, app->cfg.pen_text);
    draw_text(app->rp, 8, app->status_y, app->status, app->cfg.pen_text);
}

void gui_draw_chat(struct MC4App *app)
{
    int i;
    WORD y = app->chat_y;
    WORD ww;
    WORD chat_bottom;
    WORD input_y;

    ww = app->win->GZZWidth ? app->win->GZZWidth : app->win->Width;
    chat_bottom = (WORD)(app->chat_y + (MC4_CHAT_LINES * 10) + 16);
    input_y = (WORD)(app->chat_y + (MC4_CHAT_LINES * 10) + 4);

    fill_rect(app->rp, 4, y - 10, ww - 8, chat_bottom, app->cfg.pen_bg);
    frame_rect(app->rp, 4, y - 11, ww - 8, chat_bottom, app->cfg.pen_text);
    Move(app->rp, 4, input_y - 10); Draw(app->rp, ww - 8, input_y - 10);
    for (i = 0; i < app->chat_count; ++i) {
        draw_text(app->rp, 8, y, app->chat_lines[i], app->cfg.pen_text);
        y += 10;
    }
    draw_text(app->rp, 8, input_y, ">", app->cfg.pen_text);
    draw_text(app->rp, 20, input_y, app->chat_input, app->cfg.pen_text);
}

void gui_draw_all(struct MC4App *app)
{
    if (!app->win)
        return;
    {
        WORD ww = app->win->GZZWidth ? app->win->GZZWidth : app->win->Width;
        WORD wh = app->win->GZZHeight ? app->win->GZZHeight : app->win->Height;
        fill_rect(app->rp, 0, 0, ww - 1, wh - 1, app->cfg.pen_bg);
    }
    if (app->view == MC4_VIEW_LOBBY)
        gui_draw_lobby(app);
    else
        draw_board(app);
    gui_draw_status(app);
    gui_draw_chat(app);
}

void gui_animate_drop(struct MC4App *app, int col, int row, UBYTE player)
{
    int step;
    int previous = -1;

    if (!app->cfg.animation_enabled) {
        gui_draw_cell(app, row, col, player);
        return;
    }

    for (step = 0; step <= row; ++step) {
        if (previous >= 0)
            gui_draw_cell(app, previous, col, MC4_EMPTY);
        if (step == row)
            gui_draw_cell(app, row, col, player);
        else
            gui_draw_cell(app, step, col, player);
        previous = step;
        Delay(1);
    }
}
