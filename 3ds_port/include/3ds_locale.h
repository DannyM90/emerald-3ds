#ifndef CTR_LOCALE_H
#define CTR_LOCALE_H

#include "constants/global.h"

#if GAME_LANGUAGE == LANGUAGE_SPANISH
#define CTR_TEXT(english, spanish, japanese) (spanish)
#elif GAME_LANGUAGE == LANGUAGE_JAPANESE
#define CTR_TEXT(english, spanish, japanese) (japanese)
#else
#define CTR_TEXT(english, spanish, japanese) (english)
#endif

#endif
