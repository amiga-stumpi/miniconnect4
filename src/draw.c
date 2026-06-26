#include <proto/graphics.h>
#include <proto/dos.h>
#include "miniconnect4.h"

#define PEN_BG 0
#define PEN_TEXT 1
#define PEN_BOARD 1
#define PEN_P1 2
#define PEN_P2 3
#define PEN_GRID 1

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
        pen = PEN_P1;
    else if (value == MC4_P2)
        pen = PEN_P2;
    else
        pen = PEN_BG;

    rr = r * r;
    SetAPen(rp, pen);
    for (y = -r; y <= r; ++y) {
        for (x = -r; x <= r; ++x) {
            if ((x * x) + (y * y) <= rr)
                WritePixel(rp, cx + x, cy + y);
        }
    }
    SetAPen(rp, PEN_GRID);
    for (x = -r; x <= r; ++x) {
        WritePixel(rp, cx + x, cy - r);
        WritePixel(rp, cx + x, cy + r);
    }
}

void gui_draw_cell(struct MC4App *app, int row, int col, UBYTE value)
{
    WORD x = app->board_x + (WORD)(col * app->cell);
    WORD y = app->board_y + (WORD)(row * app->cell);
    WORD r = (WORD)((app->cell / 2) - 3);

    fill_rect(app->rp, x + 1, y + 1, x + app->cell - 2, y + app->cell - 2, PEN_BOARD);
    draw_disc(app, (WORD)(x + app->cell / 2), (WORD)(y + app->cell / 2), r, value);
}

static void draw_board(struct MC4App *app)
{
    int r, c;
    fill_rect(app->rp, app->board_x, app->board_y,
              app->board_x + app->board_w, app->board_y + app->board_h, PEN_BOARD);
    for (r = 0; r < MC4_ROWS; ++r)
        for (c = 0; c < MC4_COLS; ++c)
            gui_draw_cell(app, r, c, app->game.board[r][c]);
    frame_rect(app->rp, app->board_x, app->board_y,
               app->board_x + app->board_w, app->board_y + app->board_h, PEN_TEXT);
}

void gui_draw_status(struct MC4App *app)
{
    fill_rect(app->rp, 4, app->status_y - 10, app->win->Width - 8, app->status_y + 4, PEN_BG);
    draw_text(app->rp, 8, app->status_y, app->status, PEN_TEXT);
}

void gui_draw_chat(struct MC4App *app)
{
    int i;
    WORD y = app->chat_y;
    if (!app->cfg.chat_enabled)
        return;
    fill_rect(app->rp, 4, y - 10, app->win->Width - 8, app->win->Height - 6, PEN_BG);
    for (i = 0; i < app->chat_count; ++i) {
        draw_text(app->rp, 8, y, app->chat_lines[i], PEN_TEXT);
        y += 10;
    }
    draw_text(app->rp, 8, y + 2, ">", PEN_TEXT);
    draw_text(app->rp, 20, y + 2, app->chat_input, PEN_TEXT);
}

void gui_draw_all(struct MC4App *app)
{
    if (!app->win)
        return;
    fill_rect(app->rp, 0, 0, app->win->Width - 1, app->win->Height - 1, PEN_BG);
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
