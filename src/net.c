#include <exec/types.h>
#include <exec/libraries.h>
#include <proto/exec.h>
#include <proto/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include "miniconnect4.h"

#define NET_BUF 256
#ifndef FIONBIO
#define FIONBIO 0x8004667eUL
#endif

struct Library *SocketBase = 0;
static int g_listen_fd = -1;
static int g_fd = -1;
static char g_inbuf[NET_BUF];
static int g_inlen;
static LONG g_one = 1;

static void make_nonblock(int fd)
{
    g_one = 1;
    IoctlSocket(fd, FIONBIO, (char *)&g_one);
}

int net_init(void)
{
    if (SocketBase)
        return 1;
    SocketBase = OpenLibrary((STRPTR)"bsdsocket.library", 4);
    if (!SocketBase)
        SocketBase = OpenLibrary((STRPTR)"bsdsocket.library", 1);
    return SocketBase != 0;
}

void net_shutdown(void)
{
    if (SocketBase) {
        CloseLibrary(SocketBase);
        SocketBase = 0;
    }
}

void net_close(struct MC4App *app)
{
    if (g_fd >= 0) {
        CloseSocket(g_fd);
        g_fd = -1;
    }
    if (g_listen_fd >= 0) {
        CloseSocket(g_listen_fd);
        g_listen_fd = -1;
    }
    g_inlen = 0;
    app->net_state = MC4_NET_OFFLINE;
    app->net_mode = MC4_MODE_LOCAL;
    app->my_turn = 1;
}

static int send_line(const char *line)
{
    char out[NET_BUF];
    int len;
    if (g_fd < 0)
        return 0;
    util_copy(out, sizeof(out), line);
    util_append(out, sizeof(out), "\n");
    len = util_len(out);
    return send(g_fd, out, len, 0) == len;
}

int net_host(struct MC4App *app)
{
    struct sockaddr_in sa;
    if (!net_init()) {
        gui_set_status(app, "bsdsocket.library not found");
        return 0;
    }
    net_close(app);
    g_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_listen_fd < 0) {
        gui_set_status(app, "socket failed");
        return 0;
    }
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(app->cfg.port);
    sa.sin_addr.s_addr = 0;
    if (bind(g_listen_fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        gui_set_status(app, "bind failed");
        net_close(app);
        return 0;
    }
    listen(g_listen_fd, 1);
    make_nonblock(g_listen_fd);
    app->net_mode = MC4_MODE_HOST;
    app->net_state = MC4_NET_LISTENING;
    app->game.local_player = MC4_P1;
    app->my_turn = 1;
    gui_set_status(app, "Hosting: waiting for player");
    return 1;
}

int net_connect(struct MC4App *app, const char *host, UWORD port)
{
    struct hostent *he;
    struct sockaddr_in sa;
    if (!net_init()) {
        gui_set_status(app, "bsdsocket.library not found");
        return 0;
    }
    net_close(app);
    he = gethostbyname((STRPTR)host);
    if (!he || !he->h_addr) {
        gui_set_status(app, "resolve failed");
        return 0;
    }
    g_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_fd < 0) {
        gui_set_status(app, "socket failed");
        return 0;
    }
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    memcpy(&sa.sin_addr, he->h_addr, 4);
    if (connect(g_fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        gui_set_status(app, "connect failed");
        net_close(app);
        return 0;
    }
    make_nonblock(g_fd);
    app->net_mode = MC4_MODE_CLIENT;
    app->net_state = MC4_NET_CONNECTED;
    app->game.local_player = MC4_P2;
    app->my_turn = 0;
    send_line("HELLO MiniConnect4");
    gui_set_status(app, "Connected: remote turn");
    return 1;
}

static void handle_bytes(struct MC4App *app, char *buf, int n)
{
    int i;
    for (i = 0; i < n; ++i) {
        if (buf[i] == '\r')
            continue;
        if (buf[i] == '\n') {
            g_inbuf[g_inlen] = 0;
            if (g_inlen > 0)
                protocol_handle_line(app, g_inbuf);
            g_inlen = 0;
        } else if (g_inlen < NET_BUF - 1) {
            g_inbuf[g_inlen++] = buf[i];
        }
    }
}

void net_poll(struct MC4App *app)
{
    char buf[64];
    int n;
    if (app->net_state == MC4_NET_LISTENING && g_listen_fd >= 0) {
        g_fd = accept(g_listen_fd, 0, 0);
        if (g_fd >= 0) {
            CloseSocket(g_listen_fd);
            g_listen_fd = -1;
            make_nonblock(g_fd);
            app->net_state = MC4_NET_CONNECTED;
            app->net_mode = MC4_MODE_HOST;
            app->game.local_player = MC4_P1;
            app->my_turn = 1;
            send_line("HELLO MiniConnect4");
            gui_set_status(app, "Connected: your turn");
        }
    }
    if (app->net_state != MC4_NET_CONNECTED || g_fd < 0)
        return;
    for (;;) {
        n = recv(g_fd, buf, sizeof(buf), 0);
        if (n > 0) {
            handle_bytes(app, buf, n);
            continue;
        }
        if (n == 0) {
            gui_set_status(app, "Disconnected");
            net_close(app);
        }
        break;
    }
}

int net_send_move(struct MC4App *app, int col)
{
    char line[16];
    if (app->net_state != MC4_NET_CONNECTED)
        return 0;
    util_copy(line, sizeof(line), "MOVE ");
    line[5] = (char)('0' + col);
    line[6] = 0;
    return send_line(line);
}

int net_send_chat(struct MC4App *app, const char *text)
{
    char line[NET_BUF];
    if (app->net_state != MC4_NET_CONNECTED)
        return 0;
    util_copy(line, sizeof(line), "CHAT ");
    util_append(line, sizeof(line), text);
    return send_line(line);
}

int net_send_newgame(struct MC4App *app)
{
    if (app->net_state != MC4_NET_CONNECTED)
        return 0;
    return send_line("NEWGAME");
}
