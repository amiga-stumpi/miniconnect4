#ifndef MC4_SOUND_H
#define MC4_SOUND_H

#define MC4_SOUND_MOVE 0
#define MC4_SOUND_INVITE 1
#define MC4_SOUND_START 2
#define MC4_SOUND_WIN 3
#define MC4_SOUND_LOSE 4

void sound_init(void);
void sound_shutdown(void);
void sound_play(int id);
const char *sound_status(void);

#endif
