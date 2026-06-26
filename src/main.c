#include <exec/types.h>
#include <intuition/intuition.h>
#include <libraries/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include "miniconnect4.h"

ULONG __stack = 32000;

extern void chat_submit(struct MC4App *app);

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
    if (game_check_winner(&app->game, row, col)) {
        util_copy(msg, sizeof(msg), "Player ");
        util_append(msg, sizeof(msg), player == MC4_P1 ? "1 wins" : "2 wins");
        gui_set_status(app, msg);
    } else if (app->game.game_over) {
        gui_set_status(app, "Draw");
    } else {
        game_switch_player(&app->game);
        if (send_net && app->net_state == MC4_NET_CONNECTED) {
            net_send_move(app, col);
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
        if (item == MC4_NETWORK_HOST) {
            app_new_game(app);
            net_host(app);
        } else if (item == MC4_NETWORK_JOIN) {
            app_new_game(app);
            net_connect(app, app->cfg.host, app->cfg.port);
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

static void handle_key(struct MC4App *app, UWORD code)
{
    char c = 0;
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
    if (code >= 0x02 && code <= 0x0a)
        c = (char)('1' + (code - 0x02));
    else if (code >= 0x10 && code <= 0x19)
        c = (char)('q' + (code - 0x10));
    else if (code >= 0x20 && code <= 0x28)
        c = (char)('a' + (code - 0x20));
    else if (code >= 0x31 && code <= 0x38)
        c = (char)('z' + (code - 0x31));
    else if (code == 0x40)
        c = ' ';

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
                col = gui_hit_column(app, mx, my);
                if (col >= 0)
                    app_local_move(app, col, 1);
            } else if (classv == IDCMP_RAWKEY) {
                handle_key(app, code);
            } else if (classv == IDCMP_MENUPICK) {
                if (code != MENUNULL)
                    do_menu(app, MENUNUM(code), ITEMNUM(code));
            }
        }
        net_poll(app);
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
