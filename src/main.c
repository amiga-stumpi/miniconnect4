#include <exec/types.h>
#include <intuition/intuition.h>
#include <libraries/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include "miniconnect4.h"

ULONG __stack = 32000;

extern void chat_submit(struct MC4App *app);

static void clear_invite_pending(struct MC4App *app)
{
    app->invite_pending = 0;
    app->invite_wait_ticks = 0;
    app->invite_name[0] = 0;
}

static void check_invite_timeout(struct MC4App *app)
{
    if (!app->invite_pending)
        return;
    if (app->invite_wait_ticks < 1500) {
        ++app->invite_wait_ticks;
        return;
    }
    clear_invite_pending(app);
    gui_set_status(app, "Keine Antwort - Lobby bereit");
}

static int is_default_lobby(const char *s)
{
    int i = 0;
    if (!s || !*s)
        return 0;
    while (MC4_DEFAULT_LOBBY[i]) {
        if (s[i] != MC4_DEFAULT_LOBBY[i])
            return 0;
        ++i;
    }
    return s[i] == 0;
}

void app_new_game(struct MC4App *app)
{
    UBYTE local = app->game.local_player;
    game_init(&app->game);
    app->game.local_player = local ? local : MC4_P1;
    app->my_turn = (app->net_state != MC4_NET_CONNECTED || app->game.local_player == MC4_P1) ? 1 : 0;
    util_copy(app->status, sizeof(app->status), app->my_turn ? "Your turn" : "Remote turn");
    gui_layout(app);
    gui_draw_all(app);
}

void app_local_move(struct MC4App *app, int col, int send_net)
{
    WORD row;
    UBYTE player;
    char msg[64];
    int sent_net = 0;

    if (app->game.game_over)
        return;
    if (app->net_state == MC4_NET_CONNECTED && send_net && !app->my_turn) {
        gui_set_status(app, "Wait for remote move");
        return;
    }
    player = app->game.current_player;
    if (!game_drop(&app->game, col, player, &row)) {
        gui_set_status(app, "Column full");
        return;
    }
    gui_animate_drop(app, col, row, player);
    if (send_net && app->net_state == MC4_NET_CONNECTED) {
        net_send_move(app, col);
        sent_net = 1;
    }
    if (game_check_winner(&app->game, row, col)) {
        if (app->net_state == MC4_NET_CONNECTED) {
            if (player == app->game.local_player)
                gui_set_status(app, "Du hast gewonnen!");
            else
                gui_set_status(app, "Leider verloren!");
        } else {
            util_copy(msg, sizeof(msg), "Player ");
            util_append(msg, sizeof(msg), player == MC4_P1 ? "1 wins" : "2 wins");
            gui_set_status(app, msg);
        }
    } else if (app->game.game_over) {
        gui_set_status(app, "Draw");
    } else {
        game_switch_player(&app->game);
        if (sent_net) {
            app->my_turn = 0;
            gui_set_status(app, "Remote turn");
        } else {
            if (app->net_state == MC4_NET_CONNECTED) {
                app->my_turn = 1;
                gui_set_status(app, "Your turn");
            } else {
                gui_set_status(app, "Local game");
            }
        }
    }
    gui_draw_status(app);
}

static void do_menu(struct MC4App *app, UWORD menu, UWORD item)
{
    if (menu == MC4_MENU_PROJECT) {
        if (item == MC4_PROJECT_NEW) {
            app_new_game(app);
            net_send_newgame(app);
        } else if (item == MC4_PROJECT_QUIT) {
            app->running = 0;
        }
    } else if (menu == MC4_MENU_NETWORK) {
        if (item == MC4_NETWORK_LOBBY) {
            if (!net_connect_lobby(app, app->cfg.lobby, app->cfg.port)) {
                if (!is_default_lobby(app->cfg.lobby)) {
                    gui_set_status(app, "Lobby fallback...");
                    net_connect_lobby(app, MC4_DEFAULT_LOBBY, app->cfg.port);
                }
            }
        } else if (item == MC4_NETWORK_DISCONNECT) {
            net_close(app);
            gui_set_status(app, "Offline local game");
        }
    } else if (menu == MC4_MENU_OPTIONS) {
        if (item == MC4_OPTIONS_NAME) {
            gui_edit_player_name(app);
        } else if (item == MC4_OPTIONS_CHAT) {
            app->cfg.chat_enabled = app->cfg.chat_enabled ? 0 : 1;
            gui_layout(app);
            gui_draw_all(app);
        }
    } else if (menu == MC4_MENU_HELP) {
        if (item == MC4_HELP_INFO)
            gui_info(app);
    }
}

static char rawkey_to_char(UWORD code)
{
    switch (code) {
        case 0x02: return '1'; case 0x03: return '2'; case 0x04: return '3';
        case 0x05: return '4'; case 0x06: return '5'; case 0x07: return '6';
        case 0x08: return '7'; case 0x09: return '8'; case 0x0a: return '9';
        case 0x0b: return '0';
        case 0x10: return 'q'; case 0x11: return 'w'; case 0x12: return 'e';
        case 0x13: return 'r'; case 0x14: return 't'; case 0x15: return 'y';
        case 0x16: return 'u'; case 0x17: return 'i'; case 0x18: return 'o';
        case 0x19: return 'p';
        case 0x20: return 'a'; case 0x21: return 's'; case 0x22: return 'd';
        case 0x23: return 'f'; case 0x24: return 'g'; case 0x25: return 'h';
        case 0x26: return 'j'; case 0x27: return 'k'; case 0x28: return 'l';
        case 0x31: return 'z'; case 0x32: return 'x'; case 0x33: return 'c';
        case 0x34: return 'v'; case 0x35: return 'b'; case 0x36: return 'n';
        case 0x37: return 'm';
        case 0x38: return ','; case 0x39: return '.'; case 0x3a: return '/';
        case 0x40: return ' ';
    }
    return 0;
}

static void handle_key(struct MC4App *app, UWORD code)
{
    char c;

    if (code & 0x80)
        return;
    if (code == 0x44) {
        chat_submit(app);
        return;
    }
    if (code == 0x41) {
        if (app->chat_len > 0) {
            app->chat_input[--app->chat_len] = 0;
            gui_draw_chat(app);
        }
        return;
    }

    c = rawkey_to_char(code);
    if (c && app->cfg.chat_enabled && app->chat_len < MC4_CHAT_LEN - 1) {
        app->chat_input[app->chat_len++] = c;
        app->chat_input[app->chat_len] = 0;
        gui_draw_chat(app);
    }
}

static void event_loop(struct MC4App *app)
{
    ULONG sigmask;
    ULONG sigs;
    struct IntuiMessage *msg;
    ULONG classv;
    UWORD code;
    WORD mx, my;
    int col;
    int player;

    sigmask = 1L << app->win->UserPort->mp_SigBit;
    (void)sigmask;
    while (app->running) {
        sigs = SetSignal(0L, 0L);
        if (sigs & SIGBREAKF_CTRL_C)
            app->running = 0;
        while ((msg = (struct IntuiMessage *)GetMsg(app->win->UserPort))) {
            classv = msg->Class;
            code = msg->Code;
            mx = app->win->GZZMouseX;
            my = app->win->GZZMouseY;
            ReplyMsg((struct Message *)msg);
            if (classv == IDCMP_CLOSEWINDOW) {
                app->running = 0;
            } else if (classv == IDCMP_NEWSIZE) {
                gui_enforce_aspect(app);
                gui_layout(app);
                gui_draw_all(app);
            } else if (classv == IDCMP_REFRESHWINDOW) {
                BeginRefresh(app->win);
                gui_draw_all(app);
                EndRefresh(app->win, TRUE);
            } else if (classv == IDCMP_MOUSEBUTTONS && code == SELECTDOWN) {
                player = gui_hit_lobby_player(app, mx, my);
                if (player >= 0) {
                    if (net_send_invite(app, app->lobby_players[player].name)) {
                        util_copy(app->invite_name, sizeof(app->invite_name), app->lobby_players[player].name);
                        app->invite_pending = 1;
                        app->invite_wait_ticks = 0;
                        gui_set_status(app, "Anfrage gesendet");
                    } else {
                        gui_set_status(app, "Anfrage fehlgeschlagen");
                    }
                } else {
                    col = gui_hit_column(app, mx, my);
                    if (col >= 0)
                        app_local_move(app, col, 1);
                }
            } else if (classv == IDCMP_RAWKEY) {
                handle_key(app, code);
            } else if (classv == IDCMP_MENUPICK) {
                if (code != MENUNULL)
                    do_menu(app, MENUNUM(code), ITEMNUM(code));
            }
        }
        net_poll(app);
        check_invite_timeout(app);
        Delay(1);
    }
}

int main(void)
{
    struct MC4App app;
    int i;

    for (i = 0; i < (int)sizeof(app); ++i)
        ((char *)&app)[i] = 0;

    config_load(&app.cfg);
    game_init(&app.game);
    app.running = 1;
    app.my_turn = 1;
    app.view = MC4_VIEW_GAME;
    util_copy(app.status, sizeof(app.status), "Offline local game");

    if (!gui_open(&app)) {
        Write(Output(), "MiniConnect4: window open failed\n", 33);
        return 20;
    }

    event_loop(&app);
    config_save(&app.cfg);
    net_close(&app);
    net_shutdown();
    gui_close(&app);
    return 0;
}
