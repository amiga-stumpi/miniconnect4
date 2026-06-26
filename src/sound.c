#include <exec/types.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <devices/audio.h>
#include <proto/exec.h>
#include <clib/alib_protos.h>
#include "sound.h"
#include "sound_data.h"

#define SOUND_COUNT 5
#define SOUND_PERIOD_8KHZ 447
#define SOUND_VOLUME 48

static struct MsgPort *g_audio_port;
static struct IOAudio *g_audio_alloc_io;
static struct IOAudio *g_audio_write_io;
static BYTE *g_chip_data[SOUND_COUNT];
static ULONG g_chip_len[SOUND_COUNT];
static UBYTE g_audio_open;
static UBYTE g_audio_busy;
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
    static UBYTE channels[4] = { 1, 2, 4, 8 };

    if (g_audio_open)
        return;
    if (!copy_chip_samples())
        return;

    set_status("sound init");
    g_audio_port = CreatePort(0, 0);
    if (!g_audio_port) {
        set_status("sound no port");
        free_chip_samples();
        return;
    }
    g_audio_alloc_io = (struct IOAudio *)CreateExtIO(g_audio_port, sizeof(struct IOAudio));
    g_audio_write_io = (struct IOAudio *)CreateExtIO(g_audio_port, sizeof(struct IOAudio));
    if (!g_audio_alloc_io || !g_audio_write_io) {
        set_status("sound no io");
        if (g_audio_write_io)
            DeleteExtIO((struct IORequest *)g_audio_write_io);
        if (g_audio_alloc_io)
            DeleteExtIO((struct IORequest *)g_audio_alloc_io);
        DeletePort(g_audio_port);
        g_audio_write_io = 0;
        g_audio_alloc_io = 0;
        g_audio_port = 0;
        free_chip_samples();
        return;
    }

    g_audio_alloc_io->ioa_Request.io_Message.mn_Node.ln_Pri = ADALLOC_MAXPREC;
    g_audio_alloc_io->ioa_Data = channels;
    g_audio_alloc_io->ioa_Length = sizeof(channels);
    if (OpenDevice((STRPTR)AUDIONAME, 0, (struct IORequest *)g_audio_alloc_io, 0) != 0) {
        set_status("sound open failed");
        DeleteExtIO((struct IORequest *)g_audio_write_io);
        DeleteExtIO((struct IORequest *)g_audio_alloc_io);
        DeletePort(g_audio_port);
        g_audio_write_io = 0;
        g_audio_alloc_io = 0;
        g_audio_port = 0;
        free_chip_samples();
        return;
    }
    *g_audio_write_io = *g_audio_alloc_io;
    g_audio_open = 1;
    g_audio_busy = 0;
    set_status("sound ready");
}

void sound_shutdown(void)
{
    if (g_audio_write_io) {
        if (g_audio_busy && !CheckIO((struct IORequest *)g_audio_write_io)) {
            AbortIO((struct IORequest *)g_audio_write_io);
            WaitIO((struct IORequest *)g_audio_write_io);
        } else if (g_audio_busy) {
            WaitIO((struct IORequest *)g_audio_write_io);
        }
        DeleteExtIO((struct IORequest *)g_audio_write_io);
        g_audio_write_io = 0;
    }
    if (g_audio_alloc_io) {
        if (g_audio_open)
            CloseDevice((struct IORequest *)g_audio_alloc_io);
        DeleteExtIO((struct IORequest *)g_audio_alloc_io);
        g_audio_alloc_io = 0;
    }
    if (g_audio_port) {
        DeletePort(g_audio_port);
        g_audio_port = 0;
    }
    g_audio_open = 0;
    g_audio_busy = 0;
    free_chip_samples();
}

void sound_play(int id)
{
    if (!g_audio_open || !g_audio_write_io) {
        set_status("sound not open");
        return;
    }
    if (id < 0 || id >= SOUND_COUNT || !g_chip_data[id]) {
        set_status("sound bad id");
        return;
    }

    if (g_audio_busy) {
        if (!CheckIO((struct IORequest *)g_audio_write_io)) {
            AbortIO((struct IORequest *)g_audio_write_io);
            WaitIO((struct IORequest *)g_audio_write_io);
        } else {
            WaitIO((struct IORequest *)g_audio_write_io);
        }
        g_audio_busy = 0;
    }

    g_audio_write_io->ioa_Request.io_Command = CMD_WRITE;
    g_audio_write_io->ioa_Request.io_Flags = 0;
    g_audio_write_io->ioa_Data = (UBYTE *)g_chip_data[id];
    g_audio_write_io->ioa_Length = g_chip_len[id];
    g_audio_write_io->ioa_Period = SOUND_PERIOD_8KHZ;
    g_audio_write_io->ioa_Volume = SOUND_VOLUME;
    g_audio_write_io->ioa_Cycles = 1;
    SendIO((struct IORequest *)g_audio_write_io);
    g_audio_busy = 1;
    set_status("sound write sent");
}
