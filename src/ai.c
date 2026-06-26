#include "miniconnect4.h"

#define WIN_SCORE 12000
#define INF_SCORE 30000

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
    int i;
    int col;
    for (i = 0; i < MC4_COLS; ++i) {
        col = center_order[i];
        if (game_can_play(game, col) && simulate_win(game, col, player))
            return col;
    }
    return -1;
}

static int count_immediate_wins(const struct MC4Game *game, UBYTE player)
{
    int c;
    int n = 0;
    for (c = 0; c < MC4_COLS; ++c) {
        if (game_can_play(game, c) && simulate_win(game, c, player))
            ++n;
    }
    return n;
}

static int score_window(int own, int opp, int empty)
{
    if (own && opp)
        return 0;
    if (own == 4)
        return 5000;
    if (own == 3 && empty == 1)
        return 220;
    if (own == 2 && empty == 2)
        return 45;
    if (own == 1 && empty == 3)
        return 6;
    if (opp == 4)
        return -5000;
    if (opp == 3 && empty == 1)
        return -260;
    if (opp == 2 && empty == 2)
        return -55;
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
            score += 18;
        else if (game->board[r][3] == opp)
            score -= 18;
    }

    score += count_immediate_wins(game, player) * 900;
    score -= count_immediate_wins(game, opp) * 950;

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

static int minimax(struct MC4Game *game, UBYTE ai, UBYTE turn, int depth, int alpha, int beta)
{
    int i;
    int col;
    WORD row;
    int score;
    int best;
    struct MC4Game tmp;
    UBYTE opp = other_player(ai);

    if (game->game_over || depth <= 0)
        return score_position(game, ai);

    if (turn == ai) {
        best = -INF_SCORE;
        for (i = 0; i < MC4_COLS; ++i) {
            col = center_order[i];
            if (!game_can_play(game, col))
                continue;
            tmp = *game;
            game_drop(&tmp, col, turn, &row);
            if (game_check_winner(&tmp, row, col))
                score = WIN_SCORE + depth;
            else
                score = minimax(&tmp, ai, opp, depth - 1, alpha, beta);
            if (score > best)
                best = score;
            if (best > alpha)
                alpha = best;
            if (beta <= alpha)
                break;
        }
        return best == -INF_SCORE ? score_position(game, ai) : best;
    }

    best = INF_SCORE;
    for (i = 0; i < MC4_COLS; ++i) {
        col = center_order[i];
        if (!game_can_play(game, col))
            continue;
        tmp = *game;
        game_drop(&tmp, col, turn, &row);
        if (game_check_winner(&tmp, row, col))
            score = -WIN_SCORE - depth;
        else
            score = minimax(&tmp, ai, ai, depth - 1, alpha, beta);
        if (score < best)
            best = score;
        if (best < beta)
            beta = best;
        if (beta <= alpha)
            break;
    }
    return best == INF_SCORE ? score_position(game, ai) : best;
}

static int search_move(const struct MC4Game *game, UBYTE player, int depth)
{
    int i;
    int col;
    WORD row;
    int score;
    int best_score = -INF_SCORE;
    int best_col = -1;
    struct MC4Game tmp;

    for (i = 0; i < MC4_COLS; ++i) {
        col = center_order[i];
        if (!game_can_play(game, col))
            continue;
        tmp = *game;
        game_drop(&tmp, col, player, &row);
        if (game_check_winner(&tmp, row, col))
            score = WIN_SCORE + depth;
        else
            score = minimax(&tmp, player, other_player(player), depth - 1, -INF_SCORE, INF_SCORE);
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
        col = search_move(game, player, 5);
        if (col >= 0)
            return col;
    } else if (level >= MC4_AI_MEDIUM) {
        col = search_move(game, player, 3);
        if (col >= 0)
            return col;
    }

    return first_playable(game);
}
