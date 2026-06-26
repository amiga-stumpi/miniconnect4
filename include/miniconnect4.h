#ifndef MINICONNECT4_H
#define MINICONNECT4_H

#include <exec/types.h>
#include <intuition/intuition.h>
#include <graphics/rastport.h>

#define MC4_VERSION "0.1"
#define MC4_NAME "MiniConnect4"

#define MC4_COLS 7
#define MC4_ROWS 6
#define MC4_CHAT_LINES 4
#define MC4_LOBBY_MAX_PLAYERS 16
#define MC4_CHAT_LEN 96
#define MC4_NAME_LEN 31
#define MC4_HOST_LEN 95
#define MC4_PORT 4544
#define MC4_DEFAULT_LOBBY "lobby.c64.social"

#define MC4_EMPTY 0
#define MC4_P1 1
#define MC4_P2 2

#define MC4_MODE_LOCAL 0
#define MC4_MODE_HOST 1
#define MC4_MODE_CLIENT 2
#define MC4_MODE_LOBBY 3

#define MC4_NET_OFFLINE 0
#define MC4_NET_LISTENING 1
#define MC4_NET_CONNECTING 2
#define MC4_NET_CONNECTED 3

#define MC4_MENU_PROJECT 0
#define MC4_MENU_NETWORK 1
#define MC4_MENU_OPTIONS 2
#define MC4_MENU_HELP 3

#define MC4_PROJECT_NEW 0
#define MC4_PROJECT_QUIT 1
#define MC4_NETWORK_LOBBY 0
#define MC4_NETWORK_DISCONNECT 1
#define MC4_OPTIONS_NAME 0
#define MC4_OPTIONS_CHAT 1
#define MC4_HELP_INFO 0

#define MC4_VIEW_GAME 0
#define MC4_VIEW_LOBBY 1

struct MC4Config {
    WORD win_x;
    WORD win_y;
    WORD win_w;
    WORD win_h;
    char player_name[MC4_NAME_LEN + 1];
    char lobby[MC4_HOST_LEN + 1];
    UWORD port;
    UBYTE chat_enabled;
    UBYTE animation_enabled;
};

struct MC4Game {
    UBYTE board[MC4_ROWS][MC4_COLS];
    UBYTE current_player;
    UBYTE local_player;
    UBYTE winner;
    UBYTE game_over;
    UWORD move_count;
    WORD last_col;
    WORD last_row;
};

struct MC4LobbyPlayer {
    char name[MC4_NAME_LEN + 1];
    UBYTE busy;
};

struct MC4App {
    struct Window *win;
    struct Screen *screen;
    struct RastPort *rp;
    struct MC4Config cfg;
    struct MC4Game game;
    WORD board_x;
    WORD board_y;
    WORD cell;
    WORD cell_w;
    WORD cell_h;
    WORD board_w;
    WORD board_h;
    WORD status_y;
    WORD chat_y;
    char status[128];
    char remote_name[MC4_NAME_LEN + 1];
    struct MC4LobbyPlayer lobby_players[MC4_LOBBY_MAX_PLAYERS];
    UBYTE lobby_count;
    UBYTE view;
    char chat_lines[MC4_CHAT_LINES][MC4_CHAT_LEN];
    UBYTE chat_count;
    char chat_input[MC4_CHAT_LEN];
    UBYTE chat_len;
    UBYTE running;
    UBYTE need_full_redraw;
    UBYTE resize_lock;
    WORD last_win_w;
    WORD last_win_h;
    UBYTE net_mode;
    UBYTE net_state;
    UBYTE my_turn;
};

void util_copy(char *dst, int max_len, const char *src);
int util_len(const char *s);
int util_starts(const char *s, const char *prefix);
void util_append(char *dst, int max_len, const char *src);
void util_num(char *dst, int max_len, LONG v);

void config_make_default_name(char *dst, int max_len);
void config_defaults(struct MC4Config *cfg);
void config_load(struct MC4Config *cfg);
void config_save(const struct MC4Config *cfg);

void game_init(struct MC4Game *g);
int game_can_play(const struct MC4Game *g, int col);
int game_drop(struct MC4Game *g, int col, UBYTE player, WORD *row_out);
int game_check_winner(struct MC4Game *g, int row, int col);
void game_switch_player(struct MC4Game *g);

int gui_open(struct MC4App *app);
void gui_close(struct MC4App *app);
void gui_layout(struct MC4App *app);
void gui_draw_all(struct MC4App *app);
void gui_enforce_aspect(struct MC4App *app);
void gui_draw_status(struct MC4App *app);
void gui_draw_chat(struct MC4App *app);
void gui_draw_lobby(struct MC4App *app);
void gui_draw_cell(struct MC4App *app, int row, int col, UBYTE value);
void gui_animate_drop(struct MC4App *app, int col, int row, UBYTE player);
int gui_hit_column(struct MC4App *app, WORD x, WORD y);
int gui_hit_lobby_player(struct MC4App *app, WORD x, WORD y);
void gui_set_status(struct MC4App *app, const char *s);
void gui_add_chat(struct MC4App *app, const char *s);
void gui_info(struct MC4App *app);
void gui_edit_player_name(struct MC4App *app);

int net_init(void);
void net_shutdown(void);
void net_close(struct MC4App *app);
int net_connect_lobby(struct MC4App *app, const char *host, UWORD port);
void net_poll(struct MC4App *app);
int net_send_move(struct MC4App *app, int col);
int net_send_chat(struct MC4App *app, const char *text);
int net_send_newgame(struct MC4App *app);
int net_send_lobby_chat(struct MC4App *app, const char *text);
int net_send_invite(struct MC4App *app, const char *name);

void protocol_handle_line(struct MC4App *app, const char *line);
void app_new_game(struct MC4App *app);
void app_local_move(struct MC4App *app, int col, int send_net);

#endif
