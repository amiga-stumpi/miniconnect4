#include <dos/dos.h>
#include <proto/dos.h>
#include <string.h>
#include "miniconnect4.h"

#define CONFIG_FILE "MiniConnect4.conf"

static int parse_int(const char *s, int def)
{
    int v = 0;
    int neg = 0;
    if (!s || !*s)
        return def;
    if (*s == '-') {
        neg = 1;
        ++s;
    }
    while (*s >= '0' && *s <= '9') {
        v = (v * 10) + (*s - '0');
        ++s;
    }
    return neg ? -v : v;
}

static void trim_cr(char *s)
{
    int n = util_len(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[n - 1] = 0;
        --n;
    }
}

void config_defaults(struct MC4Config *cfg)
{
    cfg->win_x = 20;
    cfg->win_y = 12;
    cfg->win_w = 390;
    cfg->win_h = 220;
    util_copy(cfg->player_name, sizeof(cfg->player_name), "Player");
    util_copy(cfg->host, sizeof(cfg->host), "127.0.0.1");
    cfg->port = MC4_PORT;
    cfg->chat_enabled = 1;
    cfg->animation_enabled = 1;
}

static int read_line(BPTR f, char *line, int max_len)
{
    char ch;
    LONG got;
    int n = 0;

    if (max_len <= 1)
        return 0;

    while (n < max_len - 1) {
        got = Read(f, &ch, 1);
        if (got <= 0)
            break;
        line[n++] = ch;
        if (ch == '\n')
            break;
    }
    line[n] = 0;
    return n > 0;
}

void config_load(struct MC4Config *cfg)
{
    BPTR f;
    char line[128];
    char *eq;

    config_defaults(cfg);
    f = Open((STRPTR)CONFIG_FILE, MODE_OLDFILE);
    if (!f)
        return;

    while (read_line(f, line, sizeof(line))) {
        trim_cr(line);
        eq = line;
        while (*eq && *eq != '=')
            ++eq;
        if (*eq != '=')
            continue;
        *eq++ = 0;
        if (line[0] == '#')
            continue;
        if (!strcmp(line, "x")) cfg->win_x = (WORD)parse_int(eq, cfg->win_x);
        else if (!strcmp(line, "y")) cfg->win_y = (WORD)parse_int(eq, cfg->win_y);
        else if (!strcmp(line, "w")) cfg->win_w = (WORD)parse_int(eq, cfg->win_w);
        else if (!strcmp(line, "h")) cfg->win_h = (WORD)parse_int(eq, cfg->win_h);
        else if (!strcmp(line, "name")) util_copy(cfg->player_name, sizeof(cfg->player_name), eq);
        else if (!strcmp(line, "host")) util_copy(cfg->host, sizeof(cfg->host), eq);
        else if (!strcmp(line, "port")) cfg->port = (UWORD)parse_int(eq, MC4_PORT);
        else if (!strcmp(line, "chat")) cfg->chat_enabled = (UBYTE)parse_int(eq, 1);
        else if (!strcmp(line, "animation")) cfg->animation_enabled = (UBYTE)parse_int(eq, 1);
    }
    Close(f);
}

static void write_line(BPTR f, const char *key, const char *value)
{
    Write(f, (APTR)key, util_len(key));
    Write(f, (APTR)"=", 1);
    Write(f, (APTR)value, util_len(value));
    Write(f, (APTR)"\n", 1);
}

void config_save(const struct MC4Config *cfg)
{
    BPTR f;
    char n[16];
    f = Open((STRPTR)CONFIG_FILE, MODE_NEWFILE);
    if (!f)
        return;
    util_num(n, sizeof(n), cfg->win_x); write_line(f, "x", n);
    util_num(n, sizeof(n), cfg->win_y); write_line(f, "y", n);
    util_num(n, sizeof(n), cfg->win_w); write_line(f, "w", n);
    util_num(n, sizeof(n), cfg->win_h); write_line(f, "h", n);
    write_line(f, "name", cfg->player_name);
    write_line(f, "host", cfg->host);
    util_num(n, sizeof(n), cfg->port); write_line(f, "port", n);
    util_num(n, sizeof(n), cfg->chat_enabled); write_line(f, "chat", n);
    util_num(n, sizeof(n), cfg->animation_enabled); write_line(f, "animation", n);
    Close(f);
}
