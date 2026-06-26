#include "miniconnect4.h"

static int center_order[MC4_COLS] = { 3, 2, 4, 1, 5, 0, 6 };

static UBYTE other_player(UBYTE p)
{
    return p == MC4_P1 ? MC4_P2 : MC4_P1;
}

static int simulate_win(const struct MC4Game *game, int col, UBYTE player)
{
    struct MC4Game tmp;
    WORD row;

    tmp = *game;
    if (!game_drop(&tmp, col, player, &row))
        return 0;
    return game_check_winner(&tmp, row, col) ? 1 : 0;
}

static int first_playable(const struct MC4Game *game)
{
    int i;
    int col;
    for (i = 0; i < MC4_COLS; ++i) {
        col = center_order[i];
        if (game_can_play(game, col))
            return col;
    }
    return -1;
}

static int immediate_win(const struct MC4Game *game, UBYTE player)
{
    int col;
    for (col = 0; col < MC4_COLS; ++col) {
        if (game_can_play(game, col) && simulate_win(game, col, player))
            return col;
    }
    return -1;
}

static int makes_opponent_win(const struct MC4Game *game, int col, UBYTE player)
{
    struct MC4Game tmp;
    WORD row;
    UBYTE opp = other_player(player);
    int c;

    tmp = *game;
    if (!game_drop(&tmp, col, player, &row))
        return 1;
    if (game_check_winner(&tmp, row, col))
        return 0;
    for (c = 0; c < MC4_COLS; ++c) {
        if (game_can_play(&tmp, c) && simulate_win(&tmp, c, opp))
            return 1;
    }
    return 0;
}

static int score_window(int own, int opp, int empty)
{
    if (own && opp)
        return 0;
    if (own == 3 && empty == 1)
        return 80;
    if (own == 2 && empty == 2)
        return 20;
    if (own == 1 && empty == 3)
        return 4;
    if (opp == 3 && empty == 1)
        return -70;
    if (opp == 2 && empty == 2)
        return -12;
    return 0;
}

static int score_position(const struct MC4Game *game, UBYTE player)
{
    UBYTE opp = other_player(player);
    int score = 0;
    int r, c, i;
    int own, other, empty;
    int dr[4] = { 0, 1, 1, 1 };
    int dc[4] = { 1, 0, 1, -1 };
    int rr, cc;

    for (r = 0; r < MC4_ROWS; ++r) {
        if (game->board[r][3] == player)
            score += 6;
        else if (game->board[r][3] == opp)
            score -= 6;
    }

    for (r = 0; r < MC4_ROWS; ++r) {
        for (c = 0; c < MC4_COLS; ++c) {
            for (i = 0; i < 4; ++i) {
                int k;
                own = other = empty = 0;
                for (k = 0; k < 4; ++k) {
                    rr = r + dr[i] * k;
                    cc = c + dc[i] * k;
                    if (rr < 0 || rr >= MC4_ROWS || cc < 0 || cc >= MC4_COLS) {
                        own = other = 1;
                        break;
                    }
                    if (game->board[rr][cc] == player)
                        ++own;
                    else if (game->board[rr][cc] == opp)
                        ++other;
                    else
                        ++empty;
                }
                score += score_window(own, other, empty);
            }
        }
    }
    return score;
}

static int best_scored_move(const struct MC4Game *game, UBYTE player)
{
    int best_col = -1;
    int best_score = -32000;
    int i;
    int col;
    int score;
    WORD row;
    struct MC4Game tmp;

    for (i = 0; i < MC4_COLS; ++i) {
        col = center_order[i];
        if (!game_can_play(game, col))
            continue;
        tmp = *game;
        if (!game_drop(&tmp, col, player, &row))
            continue;
        game_check_winner(&tmp, row, col);
        score = score_position(&tmp, player);
        if (makes_opponent_win(game, col, player))
            score -= 500;
        if (score > best_score) {
            best_score = score;
            best_col = col;
        }
    }
    return best_col;
}

int ai_choose_move(const struct MC4Game *game, UBYTE player, UBYTE level)
{
    int col;
    UBYTE opp = other_player(player);

    if (level >= MC4_AI_MEDIUM) {
        col = immediate_win(game, player);
        if (col >= 0)
            return col;
        col = immediate_win(game, opp);
        if (col >= 0)
            return col;
    }

    if (level >= MC4_AI_HARD) {
        col = best_scored_move(game, player);
        if (col >= 0)
            return col;
    }

    return first_playable(game);
}
