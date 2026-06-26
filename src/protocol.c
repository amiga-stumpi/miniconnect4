#include "miniconnect4.h"

void protocol_handle_line(struct MC4App *app, const char *line)
{
    int col;
    char msg[MC4_CHAT_LEN];

    if (util_starts(line, "HELLO ")) {
        gui_add_chat(app, line + 6);
        gui_set_status(app, "Connected");
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
