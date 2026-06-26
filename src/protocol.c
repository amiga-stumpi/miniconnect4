#include "miniconnect4.h"
#include <string.h>

static int same_name(const char *a, const char *b)
{
    while (*a && *b) {
        if (*a != *b)
            return 0;
        ++a; ++b;
    }
    return *a == 0 && *b == 0;
}

static void parse_users(struct MC4App *app, const char *s)
{
    char name[MC4_NAME_LEN + 1];
    int n = 0;
    int busy = 0;
    int i = 0;
    app->lobby_count = 0;
    while (*s && app->lobby_count < MC4_LOBBY_MAX_PLAYERS) {
        n = 0;
        busy = 0;
        while (*s && *s != '|' && *s != ':') {
            if (n < MC4_NAME_LEN)
                name[n++] = *s;
            ++s;
        }
        name[n] = 0;
        if (*s == ':') {
            ++s;
            busy = (*s == '1') ? 1 : 0;
            while (*s && *s != '|')
                ++s;
        }
        if (*s == '|')
            ++s;
        if (name[0] && !same_name(name, app->cfg.player_name)) {
            i = app->lobby_count++;
            util_copy(app->lobby_players[i].name, sizeof(app->lobby_players[i].name), name);
            app->lobby_players[i].busy = busy ? 1 : 0;
        }
    }
    if (app->view == MC4_VIEW_LOBBY)
        gui_draw_lobby(app);
}


void protocol_handle_line(struct MC4App *app, const char *line)
{
    int col;
    char msg[MC4_CHAT_LEN];

    if (util_starts(line, "HELLO ")) {
        gui_add_chat(app, line + 6);
        gui_set_status(app, "Connected");
        return;
    }
    if (util_starts(line, "USERS ")) {
        parse_users(app, line + 6);
        return;
    }
    if (util_starts(line, "LOBBYCHAT ")) {
        gui_add_chat(app, line + 10);
        return;
    }
    if (util_starts(line, "INVITE ")) {
        util_copy(msg, sizeof(msg), "Challenge from ");
        util_append(msg, sizeof(msg), line + 7);
        gui_add_chat(app, msg);
        return;
    }
    if (util_starts(line, "START ")) {
        const char *p = line + 6;
        const char *role;
        int n = 0;
        while (p[n] && p[n] != ' ' && n < MC4_NAME_LEN) {
            app->remote_name[n] = p[n];
            ++n;
        }
        app->remote_name[n] = 0;
        while (*p && *p != ' ')
            ++p;
        if (*p == ' ')
            ++p;
        role = p;
        game_init(&app->game);
        app->view = MC4_VIEW_GAME;
        if (util_starts(role, "P1")) {
            app->game.local_player = MC4_P1;
            app->my_turn = 1;
            gui_set_status(app, "Game started: your turn");
        } else {
            app->game.local_player = MC4_P2;
            app->my_turn = 0;
            gui_set_status(app, "Game started: remote turn");
        }
        gui_layout(app);
        gui_draw_all(app);
        return;
    }
    if (util_starts(line, "NAME ")) {
        util_copy(app->remote_name, sizeof(app->remote_name), line + 5);
        util_copy(msg, sizeof(msg), "Remote player: ");
        util_append(msg, sizeof(msg), app->remote_name);
        gui_add_chat(app, msg);
        return;
    }
    if (util_starts(line, "MOVE ")) {
        col = line[5] - '0';
        if (col >= 0 && col < MC4_COLS) {
            app_local_move(app, col, 0);
            app->my_turn = 1;
            gui_set_status(app, "Your turn");
        }
        return;
    }
    if (util_starts(line, "CHAT ")) {
        util_copy(msg, sizeof(msg), "Remote: ");
        util_append(msg, sizeof(msg), line + 5);
        gui_add_chat(app, msg);
        return;
    }
    if (util_starts(line, "NEWGAME")) {
        app_new_game(app);
        gui_set_status(app, "New network game");
        return;
    }
    if (util_starts(line, "QUIT")) {
        gui_set_status(app, "Remote disconnected");
        net_close(app);
        return;
    }
}
