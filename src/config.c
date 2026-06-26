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

static int is_plain_player_name(const char *s)
{
    return s && strcmp(s, "Player") == 0;
}

void config_make_default_name(char *dst, int max_len)
{
    struct DateStamp ds;
    LONG n;

    DateStamp(&ds);
    n = (ds.ds_Tick + ds.ds_Minute * 37 + ds.ds_Days * 17) % 9000;
    if (n < 0)
        n = -n;
    util_copy(dst, max_len, "Player");
    util_num(dst + util_len(dst), max_len - util_len(dst), n + 1000);
}


void config_defaults(struct MC4Config *cfg)
{
    cfg->win_x = 20;
    cfg->win_y = 12;
    cfg->win_w = 390;
    cfg->win_h = 220;
    config_make_default_name(cfg->player_name, sizeof(cfg->player_name));
    util_copy(cfg->lobby, sizeof(cfg->lobby), MC4_DEFAULT_LOBBY);
    cfg->port = MC4_PORT;
    cfg->chat_enabled = 1;
    cfg->animation_enabled = 1;
    cfg->pen_bg = 0;
    cfg->pen_text = 1;
    cfg->pen_board = 1;
    cfg->pen_p1 = 2;
    cfg->pen_p2 = 3;
    cfg->vs_computer = 0;
    cfg->ai_level = MC4_AI_EASY;
    cfg->language = MC4_LANG_EN;
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
        else if (!strcmp(line, "lobby")) util_copy(cfg->lobby, sizeof(cfg->lobby), eq);
        else if (!strcmp(line, "host")) util_copy(cfg->lobby, sizeof(cfg->lobby), eq);
        else if (!strcmp(line, "port")) cfg->port = (UWORD)parse_int(eq, MC4_PORT);
        else if (!strcmp(line, "chat")) cfg->chat_enabled = (UBYTE)parse_int(eq, 1);
        else if (!strcmp(line, "animation")) cfg->animation_enabled = (UBYTE)parse_int(eq, 1);
        else if (!strcmp(line, "pen_bg")) cfg->pen_bg = (UBYTE)parse_int(eq, cfg->pen_bg);
        else if (!strcmp(line, "pen_text")) cfg->pen_text = (UBYTE)parse_int(eq, cfg->pen_text);
        else if (!strcmp(line, "pen_board")) cfg->pen_board = (UBYTE)parse_int(eq, cfg->pen_board);
        else if (!strcmp(line, "pen_p1")) cfg->pen_p1 = (UBYTE)parse_int(eq, cfg->pen_p1);
        else if (!strcmp(line, "pen_p2")) cfg->pen_p2 = (UBYTE)parse_int(eq, cfg->pen_p2);
        else if (!strcmp(line, "vs_computer")) cfg->vs_computer = (UBYTE)parse_int(eq, cfg->vs_computer);
        else if (!strcmp(line, "ai_level")) cfg->ai_level = (UBYTE)parse_int(eq, cfg->ai_level);
        else if (!strcmp(line, "language")) cfg->language = (UBYTE)parse_int(eq, cfg->language);
    }
    Close(f);
    if (!cfg->player_name[0] || is_plain_player_name(cfg->player_name))
        config_make_default_name(cfg->player_name, sizeof(cfg->player_name));
    cfg->chat_enabled = 1;
    if (cfg->ai_level < MC4_AI_EASY || cfg->ai_level > MC4_AI_HARD)
        cfg->ai_level = MC4_AI_EASY;
    cfg->vs_computer = cfg->vs_computer ? 1 : 0;
    if (cfg->language > MC4_LANG_PL)
        cfg->language = MC4_LANG_EN;
    if (cfg->pen_bg == cfg->pen_text || cfg->pen_bg == cfg->pen_board ||
        cfg->pen_bg == cfg->pen_p1 || cfg->pen_bg == cfg->pen_p2) {
        UBYTE p;
        for (p = 0; p < 16; ++p) {
            if (p != cfg->pen_text && p != cfg->pen_board &&
                p != cfg->pen_p1 && p != cfg->pen_p2) {
                cfg->pen_bg = p;
                break;
            }
        }
    }
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
    write_line(f, "lobby", cfg->lobby);
    util_num(n, sizeof(n), cfg->port); write_line(f, "port", n);
    util_num(n, sizeof(n), cfg->chat_enabled); write_line(f, "chat", n);
    util_num(n, sizeof(n), cfg->animation_enabled); write_line(f, "animation", n);
    util_num(n, sizeof(n), cfg->pen_bg); write_line(f, "pen_bg", n);
    util_num(n, sizeof(n), cfg->pen_text); write_line(f, "pen_text", n);
    util_num(n, sizeof(n), cfg->pen_board); write_line(f, "pen_board", n);
    util_num(n, sizeof(n), cfg->pen_p1); write_line(f, "pen_p1", n);
    util_num(n, sizeof(n), cfg->pen_p2); write_line(f, "pen_p2", n);
    util_num(n, sizeof(n), cfg->vs_computer); write_line(f, "vs_computer", n);
    util_num(n, sizeof(n), cfg->ai_level); write_line(f, "ai_level", n);
    util_num(n, sizeof(n), cfg->language); write_line(f, "language", n);
    Close(f);
}
