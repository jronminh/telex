/*
 * im-loop.c — the shape of an input-method frontend around telex.c.
 *
 * A keyboard grab delivers one key at a time.  For every letter the engine
 * returns the current word; the frontend keeps that word in the application
 * by sending Backspace for the suffix that changed and then the new bytes.
 * That is exactly what vnime does over zwp_input_method_v2 plus a virtual
 * keyboard; here the "application" is a screen buffer in memory, so the
 * example runs anywhere.
 *
 *   make im-loop
 *   printf 'tieengs vieetj\n' | ./examples/im-loop
 *   tiếng việt
 *
 * -v traces every edit, e.g. "..1 BS +\"ừ\"".
 *
 * The two functions to replace in a real frontend are emit_backspace() and
 * emit_text(): on Wayland they become key events on a virtual keyboard and
 * zwp_input_method_v2_commit_string().  Everything else is bookkeeping.
 */
#include "telex.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static char screen[4096];      /* what the application would show */
static size_t slen;            /* bytes used */
static size_t wstart;          /* where the word being composed starts */
static Telex t;
static int verbose;

static int u8len(unsigned char c)
{
	if (c < 0x80)
		return 1;
	if ((c & 0xe0) == 0xc0)
		return 2;
	if ((c & 0xf0) == 0xe0)
		return 3;
	if ((c & 0xf8) == 0xf0)
		return 4;
	return 1;
}

/* byte length of the common prefix, cut at a character boundary */
static size_t u8prefix(const char *a, const char *b)
{
	size_t la = strlen(a), lb = strlen(b), i = 0;

	while (i < la && i < lb) {
		int n = u8len((unsigned char)a[i]);
		if (n < 1 || i + (size_t)n > la || i + (size_t)n > lb)
			break;
		if (memcmp(a + i, b + i, (size_t)n))
			break;
		i += (size_t)n;
	}
	return i;
}

static size_t u8count(const char *s)
{
	size_t n = 0;

	while (*s) {
		int len = u8len((unsigned char)*s);
		s += len;
		n++;
	}
	return n;
}

/* in a real frontend: KEY_BACKSPACE down/up, done times */
static void emit_backspace(size_t chars)
{
	while (chars-- > 0 && slen > 0) {
		size_t i = slen - 1;
		while (i > 0 && ((unsigned char)screen[i] & 0xc0) == 0x80)
			i--;
		slen = i;
	}
	screen[slen] = '\0';
}

/* in a real frontend: zwp_input_method_v2_commit_string() */
static void emit_text(const char *text)
{
	size_t add = strlen(text);

	if (slen + add >= sizeof screen)
		add = sizeof screen - 1 - slen;
	memcpy(screen + slen, text, add);
	slen += add;
	screen[slen] = '\0';
}

/* replace the composed word on screen with `word` */
static void show(const char *word)
{
	size_t prefix = u8prefix(screen + wstart, word);
	size_t drop = u8count(screen + wstart + prefix);

	if (verbose)
		printf("  ..%zu BS +\"%s\"\n", drop, word + prefix);
	emit_backspace(drop);
	emit_text(word + prefix);
}

/* end the word: it stays in the application, the next one starts fresh */
static void finish_word(void)
{
	char out[TELEX_MAX_OUT];
	int n;

	if (t.nraw == 0)
		return;
	n = telex_flush(&t, out, sizeof out);
	if (n > 0)
		show(out);
	wstart = slen;
}

int main(int argc, char **argv)
{
	char out[TELEX_MAX_OUT];
	int ch;

	verbose = argc > 1 && !strcmp(argv[1], "-v");
	telex_init(&t, NULL);

	while ((ch = getchar()) != EOF) {
		if (ch == '\n') {
			finish_word();
			printf("%s\n", screen);
			slen = wstart = 0;
			screen[0] = '\0';
			continue;
		}

		/* Backspace inside the word goes to the engine; outside it
		 * belongs to the application and deletes the previous char. */
		if (ch == '\b' || ch == 0x7f) {
			if (telex_backspace(&t, out, sizeof out, NULL)) {
				show(out);
			} else if (slen > 0) {
				size_t i = slen - 1;
				while (i > 0 && ((unsigned char)screen[i] & 0xc0) == 0x80)
					i--;
				slen = i;
				screen[slen] = '\0';
			}
			continue;
		}

		if (isalpha(ch)) {
			telex_key(&t, ch, out, sizeof out, NULL);
			show(out);
			continue;
		}

		/* space, punctuation, Enter: commit the word, then the key */
		finish_word();
		if (slen + 1 < sizeof screen) {
			screen[slen++] = (char)ch;
			screen[slen] = '\0';
		}
		wstart = slen;
	}
	finish_word();
	if (slen)
		printf("%s\n", screen);
	return 0;
}
