#include <exec/types.h>
#include <intuition/intuition.h>
#include <libraries/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include "miniconnect4.h"

#ifndef IEQUALIFIER_LSHIFT
#define IEQUALIFIER_LSHIFT 0x0001
#endif
#ifndef IEQUALIFIER_RSHIFT
#define IEQUALIFIER_RSHIFT 0x0002
#endif

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

static char rawkey_to_char(UWORD code, UWORD qualifier)
{
    int shift = (qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) ? 1 : 0;
    char c = 0;

    switch (code) {
        case 0x01: return shift ? '!' : '1';
        case 0x02: return shift ? '"' : '2';
        case 0x03: return shift ? '#' : '3';
        case 0x04: return shift ? '$' : '4';
        case 0x05: return shift ? '%' : '5';
        case 0x06: return shift ? '&' : '6';
        case 0x07: return shift ? '/' : '7';
        case 0x08: return shift ? '(' : '8';
        case 0x09: return shift ? ')' : '9';
        case 0x0a: return shift ? '=' : '0';
        case 0x0b: return shift ? '?' : '-';
        case 0x0c: return shift ? '`' : '\'';
        case 0x1a: return shift ? '{' : '[';
        case 0x1b: return shift ? '}' : ']';
        case 0x29: return shift ? ':' : ';';
        case 0x2a: return shift ? '@' : '#';
        case 0x30: return shift ? '>' : '<';
        case 0x38: return shift ? ';' : ',';
        case 0x39: return shift ? ':' : '.';
        case 0x3a: return shift ? '_' : '/';
        case 0x40: return ' ';
        case 0x10: c = 'q'; break; case 0x11: c = 'w'; break; case 0x12: c = 'e'; break;
        case 0x13: c = 'r'; break; case 0x14: c = 't'; break; case 0x15: c = 'y'; break;
        case 0x16: c = 'u'; break; case 0x17: c = 'i'; break; case 0x18: c = 'o'; break;
        case 0x19: c = 'p'; break;
        case 0x20: c = 'a'; break; case 0x21: c = 's'; break; case 0x22: c = 'd'; break;
        case 0x23: c = 'f'; break; case 0x24: c = 'g'; break; case 0x25: c = 'h'; break;
        case 0x26: c = 'j'; break; case 0x27: c = 'k'; break; case 0x28: c = 'l'; break;
        case 0x31: c = 'z'; break; case 0x32: c = 'x'; break; case 0x33: c = 'c'; break;
        case 0x34: c = 'v'; break; case 0x35: c = 'b'; break; case 0x36: c = 'n'; break;
        case 0x37: c = 'm'; break;
    }
    if (shift && c >= 'a' && c <= 'z')
        c = (char)(c - 'a' + 'A');
    return c;
}

static void handle_key(struct MC4App *app, UWORD code, UWORD qualifier)
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

    c = rawkey_to_char(code, qualifier);
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
    UWORD qualifier;
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
            qualifier = msg->Qualifier;
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
                handle_key(app, code, qualifier);
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
