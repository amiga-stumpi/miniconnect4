#include "miniconnect4.h"

void chat_submit(struct MC4App *app)
{
    char line[MC4_CHAT_LEN];
    if (!app->cfg.chat_enabled || app->chat_len == 0)
        return;
    util_copy(line, sizeof(line), "Me: ");
    util_append(line, sizeof(line), app->chat_input);
    gui_add_chat(app, line);
    net_send_chat(app, app->chat_input);
    app->chat_input[0] = 0;
    app->chat_len = 0;
    gui_draw_chat(app);
}
