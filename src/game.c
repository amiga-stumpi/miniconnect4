#include "miniconnect4.h"

void game_init(struct MC4Game *g)
{
    int r, c;
    for (r = 0; r < MC4_ROWS; ++r)
        for (c = 0; c < MC4_COLS; ++c)
            g->board[r][c] = MC4_EMPTY;
    g->current_player = MC4_P1;
    g->local_player = MC4_P1;
    g->winner = MC4_EMPTY;
    g->game_over = 0;
    g->move_count = 0;
    g->last_col = -1;
    g->last_row = -1;
}

int game_can_play(const struct MC4Game *g, int col)
{
    if (col < 0 || col >= MC4_COLS || g->game_over)
        return 0;
    return g->board[0][col] == MC4_EMPTY;
}

int game_drop(struct MC4Game *g, int col, UBYTE player, WORD *row_out)
{
    int r;
    if (!game_can_play(g, col))
        return 0;
    for (r = MC4_ROWS - 1; r >= 0; --r) {
        if (g->board[r][col] == MC4_EMPTY) {
            g->board[r][col] = player;
            g->last_col = (WORD)col;
            g->last_row = (WORD)r;
            ++g->move_count;
            if (row_out)
                *row_out = (WORD)r;
            return 1;
        }
    }
    return 0;
}

static int count_dir(struct MC4Game *g, int row, int col, int dr, int dc, UBYTE p)
{
    int n = 0;
    row += dr;
    col += dc;
    while (row >= 0 && row < MC4_ROWS && col >= 0 && col < MC4_COLS && g->board[row][col] == p) {
        ++n;
        row += dr;
        col += dc;
    }
    return n;
}

int game_check_winner(struct MC4Game *g, int row, int col)
{
    UBYTE p;
    int dirs[4][2] = {{0,1},{1,0},{1,1},{1,-1}};
    int i;
    if (row < 0 || col < 0)
        return 0;
    p = g->board[row][col];
    if (p == MC4_EMPTY)
        return 0;
    for (i = 0; i < 4; ++i) {
        if (1 + count_dir(g, row, col, dirs[i][0], dirs[i][1], p) +
            count_dir(g, row, col, -dirs[i][0], -dirs[i][1], p) >= 4) {
            g->winner = p;
            g->game_over = 1;
            return 1;
        }
    }
    if (g->move_count >= MC4_ROWS * MC4_COLS)
        g->game_over = 1;
    return 0;
}

int game_winning_cells(const struct MC4Game *g, WORD rows[4], WORD cols[4])
{
    int dirs[4][2] = {{0,1},{1,0},{1,1},{1,-1}};
    int line_r[MC4_COLS > MC4_ROWS ? MC4_COLS : MC4_ROWS];
    int line_c[MC4_COLS > MC4_ROWS ? MC4_COLS : MC4_ROWS];
    int i, n, r, c, dr, dc, start, k;
    UBYTE p;

    if (!g || g->last_row < 0 || g->last_col < 0)
        return 0;
    p = g->board[g->last_row][g->last_col];
    if (p == MC4_EMPTY)
        return 0;

    for (i = 0; i < 4; ++i) {
        dr = dirs[i][0];
        dc = dirs[i][1];
        r = g->last_row;
        c = g->last_col;
        while (r - dr >= 0 && r - dr < MC4_ROWS && c - dc >= 0 && c - dc < MC4_COLS &&
               g->board[r - dr][c - dc] == p) {
            r -= dr;
            c -= dc;
        }
        n = 0;
        while (r >= 0 && r < MC4_ROWS && c >= 0 && c < MC4_COLS && g->board[r][c] == p && n < (int)(sizeof(line_r) / sizeof(line_r[0]))) {
            line_r[n] = r;
            line_c[n] = c;
            ++n;
            r += dr;
            c += dc;
        }
        if (n >= 4) {
            start = 0;
            for (k = 0; k < n; ++k) {
                if (line_r[k] == g->last_row && line_c[k] == g->last_col) {
                    if (k > 3)
                        start = k - 3;
                    if (start + 4 > n)
                        start = n - 4;
                    break;
                }
            }
            for (k = 0; k < 4; ++k) {
                rows[k] = (WORD)line_r[start + k];
                cols[k] = (WORD)line_c[start + k];
            }
            return 1;
        }
    }
    return 0;
}

void game_switch_player(struct MC4Game *g)
{
    g->current_player = (g->current_player == MC4_P1) ? MC4_P2 : MC4_P1;
}
