#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>

#define MAX_CLIENTS 32
#define NAME_LEN 31
#define BUF_LEN 512
#define DEFAULT_PORT 4544
#define SERVER_VERSION "1.0"

struct Client {
    int fd;
    char name[NAME_LEN + 1];
    char inbuf[BUF_LEN];
    int inlen;
    int busy;
    int peer;
    int pending_from;
    int pending_to;
    time_t pending_time;
    int rematch;
    int role;
    int rematch_starter;
};

static struct Client clients[MAX_CLIENTS];


static int daemonize(void)
{
    pid_t pid;
    int fd;

    pid = fork();
    if (pid < 0)
        return 0;
    if (pid > 0)
        exit(0);

    if (setsid() < 0)
        return 0;

    signal(SIGHUP, SIG_IGN);
    pid = fork();
    if (pid < 0)
        return 0;
    if (pid > 0)
        exit(0);

    umask(0);
    if (chdir("/") < 0)
        return 0;

    fd = open("/dev/null", O_RDWR);
    if (fd >= 0) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO)
            close(fd);
    }
    return 1;
}

static void send_line(int fd, const char *line)
{
    if (fd >= 0) {
        send(fd, line, strlen(line), 0);
        send(fd, "\n", 1, 0);
    }
}

static int find_free(void)
{
    int i;
    for (i = 0; i < MAX_CLIENTS; ++i)
        if (clients[i].fd < 0)
            return i;
    return -1;
}

static int find_name(const char *name)
{
    int i;
    for (i = 0; i < MAX_CLIENTS; ++i)
        if (clients[i].fd >= 0 && strcmp(clients[i].name, name) == 0)
            return i;
    return -1;
}

static int find_name_except(const char *name, int except)
{
    int i;
    for (i = 0; i < MAX_CLIENTS; ++i)
        if (i != except && clients[i].fd >= 0 && strcmp(clients[i].name, name) == 0)
            return i;
    return -1;
}

static void broadcast_users(void)
{
    char line[BUF_LEN];
    char item[64];
    int i, j;
    strcpy(line, "USERS ");
    for (i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i].fd < 0 || !clients[i].name[0])
            continue;
        snprintf(item, sizeof(item), "%s:%d|", clients[i].name, clients[i].busy ? 1 : 0);
        if (strlen(line) + strlen(item) < sizeof(line) - 1)
            strcat(line, item);
    }
    for (j = 0; j < MAX_CLIENTS; ++j)
        if (clients[j].fd >= 0)
            send_line(clients[j].fd, line);
}

static void close_client(int idx)
{
    int peer;
    int i;
    if (clients[idx].fd < 0)
        return;
    peer = clients[idx].peer;
    for (i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i].pending_from == idx || clients[i].pending_to == idx) {
            clients[i].pending_from = -1;
            clients[i].pending_to = -1;
            clients[i].pending_time = 0;
        }
    }
    close(clients[idx].fd);
    clients[idx].fd = -1;
    clients[idx].name[0] = 0;
    clients[idx].inlen = 0;
    clients[idx].busy = 0;
    clients[idx].peer = -1;
    clients[idx].pending_from = -1;
    clients[idx].pending_to = -1;
    clients[idx].pending_time = 0;
    clients[idx].rematch = -1;
    clients[idx].role = 0;
    clients[idx].rematch_starter = 0;
    if (peer >= 0 && clients[peer].fd >= 0) {
        clients[peer].busy = 0;
        clients[peer].peer = -1;
        clients[peer].pending_from = -1;
        clients[peer].pending_to = -1;
        clients[peer].pending_time = 0;
        clients[peer].rematch = -1;
        clients[peer].role = 0;
        clients[peer].rematch_starter = 0;
        send_line(clients[peer].fd, "QUIT");
    }
    broadcast_users();
}

static void clear_pending_pair(int a, int b)
{
    if (a >= 0 && clients[a].fd >= 0) {
        clients[a].pending_from = -1;
        clients[a].pending_to = -1;
        clients[a].pending_time = 0;
    }
    if (b >= 0 && clients[b].fd >= 0) {
        clients[b].pending_from = -1;
        clients[b].pending_to = -1;
        clients[b].pending_time = 0;
    }
}

static int pending_valid(int requester, int target)
{
    time_t now = time(0);
    if (requester < 0 || target < 0)
        return 0;
    if (clients[requester].pending_to != target || clients[target].pending_from != requester)
        return 0;
    if (now - clients[requester].pending_time > 30) {
        clear_pending_pair(requester, target);
        return 0;
    }
    return 1;
}

static void relay_to_peer(int idx, const char *line)
{
    int peer = clients[idx].peer;
    if (peer >= 0 && clients[peer].fd >= 0)
        send_line(clients[peer].fd, line);
}

static void lobby_chat(int idx, const char *text)
{
    char line[BUF_LEN];
    char short_text[384];
    int i;
    strncpy(short_text, text, sizeof(short_text) - 1);
    short_text[sizeof(short_text) - 1] = 0;
    snprintf(line, sizeof(line), "LOBBYCHAT %.31s: %s", clients[idx].name[0] ? clients[idx].name : "Player", short_text);
    for (i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i].fd >= 0 && !clients[i].busy)
            send_line(clients[i].fd, line);
    }
}

static void start_game_roles(int a, int b, int role_a, int role_b, int starter)
{
    char line[BUF_LEN];
    const char *ra = role_a == 2 ? "P2" : "P1";
    const char *rb = role_b == 1 ? "P1" : "P2";
    const char *st = starter == 2 ? "P2" : "P1";
    clear_pending_pair(a, b);
    clients[a].busy = 1;
    clients[b].busy = 1;
    clients[a].peer = b;
    clients[b].peer = a;
    clients[a].rematch = -1;
    clients[b].rematch = -1;
    clients[a].role = role_a == 2 ? 2 : 1;
    clients[b].role = role_b == 1 ? 1 : 2;
    clients[a].rematch_starter = 0;
    clients[b].rematch_starter = 0;
    snprintf(line, sizeof(line), "START %s %s %s", clients[b].name, ra, st);
    send_line(clients[a].fd, line);
    snprintf(line, sizeof(line), "START %s %s %s", clients[a].name, rb, st);
    send_line(clients[b].fd, line);
    broadcast_users();
}

static void start_game(int a, int b)
{
    start_game_roles(a, b, 1, 2, 1);
}

static void start_rematch(int a, int b)
{
    int role_a = clients[a].role;
    int role_b = clients[b].role;
    int starter = clients[a].rematch_starter ? clients[a].rematch_starter : clients[b].rematch_starter;
    if (starter != 1 && starter != 2)
        starter = 1;
    if (role_a != 1 && role_a != 2)
        role_a = 1;
    if (role_b != 1 && role_b != 2)
        role_b = role_a == 1 ? 2 : 1;
    if (role_a == role_b)
        role_b = role_a == 1 ? 2 : 1;
    start_game_roles(a, b, role_a, role_b, starter);
}


static void return_pair_to_lobby(int a, int b)
{
    if (a >= 0 && clients[a].fd >= 0) {
        clients[a].busy = 0;
        clients[a].peer = -1;
        clients[a].rematch = -1;
        clients[a].role = 0;
        clients[a].rematch_starter = 0;
        send_line(clients[a].fd, "LOBBY");
    }
    if (b >= 0 && clients[b].fd >= 0) {
        clients[b].busy = 0;
        clients[b].peer = -1;
        clients[b].rematch = -1;
        clients[b].role = 0;
        clients[b].rematch_starter = 0;
        send_line(clients[b].fd, "LOBBY");
    }
    broadcast_users();
}

static int parse_role_token(const char *s)
{
    while (*s == ' ')
        ++s;
    if (strncmp(s, "P2", 2) == 0)
        return 2;
    if (strncmp(s, "P1", 2) == 0)
        return 1;
    return 0;
}

static void handle_rematch(int idx, int yes, int starter)
{
    int peer = clients[idx].peer;
    if (peer < 0 || clients[peer].fd < 0) {
        clients[idx].busy = 0;
        clients[idx].peer = -1;
        clients[idx].rematch = -1;
        clients[idx].role = 0;
        clients[idx].rematch_starter = 0;
        send_line(clients[idx].fd, "LOBBY");
        broadcast_users();
        return;
    }
    clients[idx].rematch = yes ? 1 : 0;
    clients[idx].rematch_starter = starter;
    if (!yes) {
        return_pair_to_lobby(idx, peer);
        return;
    }
    if (clients[peer].rematch == 1)
        start_rematch(idx, peer);
    else
        send_line(clients[idx].fd, "REMATCHWAIT");
}

static void handle_line(int idx, char *line)
{
    int target;
    if (strncmp(line, "HELLO ", 6) == 0) {
        send_line(clients[idx].fd, "HELLO MiniConnect4Lobby 1.0");
    } else if (strncmp(line, "NAME ", 5) == 0) {
        char wanted[NAME_LEN + 1];
        char reply[BUF_LEN];
        snprintf(wanted, sizeof(wanted), "%.31s", line + 5);
        if (!wanted[0])
            strcpy(wanted, "Player");
        if (find_name_except(wanted, idx) >= 0) {
            snprintf(reply, sizeof(reply), "NAMETAKEN %.31s", wanted);
            send_line(clients[idx].fd, reply);
            close_client(idx);
            return;
        }
        strcpy(clients[idx].name, wanted);
        broadcast_users();
    } else if (strncmp(line, "LOBBYCHAT ", 10) == 0) {
        lobby_chat(idx, line + 10);
    } else if (strncmp(line, "INVITE ", 7) == 0) {
        target = find_name(line + 7);
        if (target >= 0 && target != idx && !clients[target].busy && !clients[idx].busy &&
            clients[idx].pending_to < 0 && clients[target].pending_from < 0) {
            char invite[BUF_LEN];
            clients[idx].pending_to = target;
            clients[idx].pending_time = time(0);
            clients[target].pending_from = idx;
            clients[target].pending_time = clients[idx].pending_time;
            snprintf(invite, sizeof(invite), "INVITE %.31s", clients[idx].name);
            send_line(clients[target].fd, invite);
        } else {
            send_line(clients[idx].fd, "INVITEFAILED Spieler nicht verfuegbar");
        }
    } else if (strncmp(line, "ACCEPT ", 7) == 0) {
        target = find_name(line + 7);
        if (target >= 0 && pending_valid(target, idx) && !clients[target].busy && !clients[idx].busy)
            start_game(target, idx);
        else
            send_line(clients[idx].fd, "INVITEFAILED Anfrage abgelaufen");
    } else if (strncmp(line, "DECLINE ", 8) == 0) {
        target = find_name(line + 8);
        if (target >= 0 && clients[target].fd >= 0) {
            char reply[BUF_LEN];
            snprintf(reply, sizeof(reply), "DECLINED %.31s", clients[idx].name);
            send_line(clients[target].fd, reply);
            clear_pending_pair(target, idx);
        }
    } else if (strncmp(line, "REMATCH YES", 11) == 0) {
        handle_rematch(idx, 1, parse_role_token(line + 11));
    } else if (strncmp(line, "REMATCH NO", 10) == 0) {
        handle_rematch(idx, 0, 0);
    } else if (strncmp(line, "MOVE ", 5) == 0 || strncmp(line, "CHAT ", 5) == 0 || strncmp(line, "NEWGAME", 7) == 0) {
        relay_to_peer(idx, line);
    } else if (strncmp(line, "QUIT", 4) == 0) {
        close_client(idx);
    }
}

static void handle_bytes(int idx, char *buf, int n)
{
    int i;
    char line[BUF_LEN];
    for (i = 0; i < n; ++i) {
        if (buf[i] == '\r')
            continue;
        if (buf[i] == '\n') {
            clients[idx].inbuf[clients[idx].inlen] = 0;
            strncpy(line, clients[idx].inbuf, sizeof(line) - 1);
            line[sizeof(line) - 1] = 0;
            clients[idx].inlen = 0;
            if (line[0])
                handle_line(idx, line);
        } else if (clients[idx].inlen < BUF_LEN - 1) {
            clients[idx].inbuf[clients[idx].inlen++] = buf[i];
        }
    }
}

int main(int argc, char **argv)
{
    int port = DEFAULT_PORT;
    int daemon_mode = 0;
    int argi;
    int listen_fd;
    int i, idx, maxfd, fd;
    int opt = 1;
    char buf[256];
    struct sockaddr_in sa;
    fd_set rfds;

    for (argi = 1; argi < argc; ++argi) {
        if (strcmp(argv[argi], "-D") == 0)
            daemon_mode = 1;
        else if (strcmp(argv[argi], "-h") == 0 || strcmp(argv[argi], "--help") == 0) {
            printf("MiniConnect4 lobby server v%s\nusage: %s [-D] [port]\n", SERVER_VERSION, argv[0]);
            return 0;
        } else
            port = atoi(argv[argi]);
    }
    if (port <= 0)
        port = DEFAULT_PORT;

    signal(SIGPIPE, SIG_IGN);
    if (daemon_mode && !daemonize()) {
        perror("daemonize");
        return 1;
    }

    for (i = 0; i < MAX_CLIENTS; ++i) {
        clients[i].fd = -1;
        clients[i].peer = -1;
        clients[i].pending_from = -1;
        clients[i].pending_to = -1;
        clients[i].rematch = -1;
        clients[i].role = 0;
        clients[i].rematch_starter = 0;
    }

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return 1;
    }
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    sa.sin_port = htons((unsigned short)port);
    if (bind(listen_fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        perror("bind");
        return 1;
    }
    if (listen(listen_fd, 8) < 0) {
        perror("listen");
        return 1;
    }
    if (!daemon_mode)
        printf("MiniConnect4 lobby server v%s listening on port %d\n", SERVER_VERSION, port);

    for (;;) {
        FD_ZERO(&rfds);
        FD_SET(listen_fd, &rfds);
        maxfd = listen_fd;
        for (i = 0; i < MAX_CLIENTS; ++i) {
            if (clients[i].fd >= 0) {
                FD_SET(clients[i].fd, &rfds);
                if (clients[i].fd > maxfd)
                    maxfd = clients[i].fd;
            }
        }
        if (select(maxfd + 1, &rfds, 0, 0, 0) < 0) {
            if (errno == EINTR)
                continue;
            perror("select");
            return 1;
        }
        if (FD_ISSET(listen_fd, &rfds)) {
            fd = accept(listen_fd, 0, 0);
            idx = find_free();
            if (fd >= 0 && idx >= 0) {
                clients[idx].fd = fd;
                clients[idx].name[0] = 0;
                clients[idx].inlen = 0;
                clients[idx].busy = 0;
                clients[idx].peer = -1;
                clients[idx].pending_from = -1;
                clients[idx].pending_to = -1;
                clients[idx].pending_time = 0;
                clients[idx].rematch = -1;
                clients[idx].role = 0;
                clients[idx].rematch_starter = 0;
                send_line(fd, "HELLO MiniConnect4Lobby 1.0");
            } else if (fd >= 0) {
                send_line(fd, "LOBBYCHAT Server: lobby full");
                close(fd);
            }
        }
        for (i = 0; i < MAX_CLIENTS; ++i) {
            if (clients[i].fd >= 0 && FD_ISSET(clients[i].fd, &rfds)) {
                int n = recv(clients[i].fd, buf, sizeof(buf), 0);
                if (n > 0)
                    handle_bytes(i, buf, n);
                else
                    close_client(i);
            }
        }
    }
}
