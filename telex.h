/*
 * telex — a small, pure-C Telex engine for a US keyboard.
 *
 * It turns US-layout Telex keystrokes into Vietnamese text:
 *
 *     tieengs  ->  tiếng
 *     vieetj   ->  việt
 *     dduowngf ->  đường
 *
 * Nothing here knows about Wayland, input methods or keyboards.  It is a
 * string/state machine; the caller decides how to deliver the result.
 */
#ifndef TELEX_H
#define TELEX_H

#include <stddef.h>

#define TELEX_MAX_KEYS 64
#define TELEX_MAX_OUT  512

typedef struct {
	int modern_style;  /* hoà/khoẻ/thuỷ instead of hòa/khỏe/thủy */
	int spell_check;   /* only accept plausible Vietnamese syllables */
	int w_at_begin;    /* a leading w becomes ư (Unikey's option) */
} TelexOptions;

typedef struct {
	TelexOptions opt;
	char raw[TELEX_MAX_KEYS];
	int nraw;
} Telex;

/* Defaults: traditional tone style, spell check on, free marking on,
 * leading w becomes ư. */
void telex_init(Telex *t, const TelexOptions *opt);
void telex_reset(Telex *t);

/* Feed one US-ASCII letter (A-Z/a-z).  Returns 1 if the key belongs to the
 * word, 0 if it is not a letter.  `out` receives the current word: the
 * composed Vietnamese form, or the raw keys if it is not Vietnamese.
 * `marked` (optional) is 1 when the result contains a Vietnamese mark. */
int telex_key(Telex *t, int ch, char *out, size_t outsz, int *marked);

/* Remove the last key of the current word.  Returns 0 if the word is empty
 * (the caller should pass Backspace to the application). */
int telex_backspace(Telex *t, char *out, size_t outsz, int *marked);

/* End the current word (space, punctuation, focus loss).  Returns the
 * number of bytes written to `out`; the word is reset. */
int telex_flush(Telex *t, char *out, size_t outsz);

/* Convert a whole word without a Telex object.  `keys` must be letters.
 * Returns the number of bytes written, or -1 if `outsz` is too small. */
int telex_convert(const char *keys, int n, const TelexOptions *opt,
		  char *out, size_t outsz, int *marked);

#endif
