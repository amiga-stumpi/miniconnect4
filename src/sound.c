#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <clib/alib_protos.h>
#include <clib/graphics_protos.h>
#include "sound.h"
#include "sound_data.h"

#define SOUND_COUNT 5
#define SOUND_PERIOD_8KHZ 447
#define SOUND_VOLUME 64
#define PAULA_CUSTOM ((volatile UWORD *)0xdff000)
#define REG_DMACON (0x096 / 2)
#define REG_AUD0LCH (0x0a0 / 2)
#define REG_AUD0LCL (0x0a2 / 2)
#define REG_AUD0LEN (0x0a4 / 2)
#define REG_AUD0PER (0x0a6 / 2)
#define REG_AUD0VOL (0x0a8 / 2)
#define REG_AUD1LCH (0x0b0 / 2)
#define REG_AUD1LCL (0x0b2 / 2)
#define REG_AUD1LEN (0x0b4 / 2)
#define REG_AUD1PER (0x0b6 / 2)
#define REG_AUD1VOL (0x0b8 / 2)

static BYTE *g_chip_data[SOUND_COUNT];
static ULONG g_chip_len[SOUND_COUNT];
static UBYTE g_sound_ready;
static char g_sound_status[64] = "sound not initialized";

static const BYTE *const g_src_data[SOUND_COUNT] = {
    sound_move_data,
    sound_invite_data,
    sound_start_data,
    sound_win_data,
    sound_lose_data
};

static const ULONG g_src_len[SOUND_COUNT] = {
    8000,
    8256,
    9600,
    12000,
    11136
};

static void set_status(const char *s)
{
    int i = 0;
    while (s[i] && i < (int)sizeof(g_sound_status) - 1) {
        g_sound_status[i] = s[i];
        ++i;
    }
    g_sound_status[i] = 0;
}

const char *sound_status(void)
{
    return g_sound_status;
}

static void paula_stop(void)
{
    PAULA_CUSTOM[REG_DMACON] = 0x0003;
}

static void paula_play_raw(BYTE *data, ULONG len, UWORD period, UWORD max_ticks)
{
    ULONG addr;
    UWORD words;
    UWORD ticks;

    if (!data || len < 2) {
        set_status("paula bad data");
        return;
    }

    addr = (ULONG)data;
    words = (UWORD)(len >> 1);
    if (words == 0)
        words = 1;

    ticks = (UWORD)(len / 160);
    if (ticks == 0)
        ticks = 1;
    if (max_ticks && ticks > max_ticks)
        ticks = max_ticks;

    paula_stop();
    WaitTOF();

    PAULA_CUSTOM[REG_AUD0LCH] = (UWORD)(addr >> 16);
    PAULA_CUSTOM[REG_AUD0LCL] = (UWORD)addr;
    PAULA_CUSTOM[REG_AUD0LEN] = words;
    PAULA_CUSTOM[REG_AUD0PER] = period;
    PAULA_CUSTOM[REG_AUD0VOL] = SOUND_VOLUME;

    PAULA_CUSTOM[REG_AUD1LCH] = (UWORD)(addr >> 16);
    PAULA_CUSTOM[REG_AUD1LCL] = (UWORD)addr;
    PAULA_CUSTOM[REG_AUD1LEN] = words;
    PAULA_CUSTOM[REG_AUD1PER] = period;
    PAULA_CUSTOM[REG_AUD1VOL] = SOUND_VOLUME;

    PAULA_CUSTOM[REG_DMACON] = 0x8203;
    Delay(ticks);
    paula_stop();
    set_status("paula sound sent");
}

static void free_chip_samples(void)
{
    int i;
    for (i = 0; i < SOUND_COUNT; ++i) {
        if (g_chip_data[i]) {
            FreeMem(g_chip_data[i], g_chip_len[i]);
            g_chip_data[i] = 0;
            g_chip_len[i] = 0;
        }
    }
}

static int copy_chip_samples(void)
{
    int i;
    for (i = 0; i < SOUND_COUNT; ++i) {
        g_chip_len[i] = g_src_len[i];
        g_chip_data[i] = (BYTE *)AllocMem(g_chip_len[i], MEMF_CHIP | MEMF_PUBLIC);
        if (!g_chip_data[i]) {
            set_status("sound no chip ram");
            free_chip_samples();
            return 0;
        }
        CopyMem((APTR)g_src_data[i], (APTR)g_chip_data[i], g_chip_len[i]);
    }
    return 1;
}

void sound_init(void)
{
    if (g_sound_ready)
        return;
    if (!copy_chip_samples())
        return;
    g_sound_ready = 1;
    set_status("paula sound ready");
}

void sound_shutdown(void)
{
    paula_stop();
    g_sound_ready = 0;
    free_chip_samples();
}

void sound_play(int id)
{
    if (!g_sound_ready) {
        set_status("sound not ready");
        return;
    }
    if (id < 0 || id >= SOUND_COUNT || !g_chip_data[id]) {
        set_status("sound bad id");
        return;
    }
    paula_play_raw(g_chip_data[id], g_chip_len[id], SOUND_PERIOD_8KHZ, 10);
}

const char *sound_test(void)
{
    BYTE *tone;
    ULONG i;
    ULONG len = 8000;

    tone = (BYTE *)AllocMem(len, MEMF_CHIP | MEMF_PUBLIC);
    if (!tone) {
        set_status("tone no chip ram");
        return sound_status();
    }
    for (i = 0; i < len; ++i)
        tone[i] = ((i / 16) & 1) ? 100 : -100;

    paula_play_raw(tone, len, SOUND_PERIOD_8KHZ, 50);
    FreeMem(tone, len);
    set_status("paula tone sent");
    return sound_status();
}
