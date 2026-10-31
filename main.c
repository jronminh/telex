/*
 * Command-line front end for the Telex engine: read US keys, write the
 * Vietnamese text.  Useful for tests and for comparing with Unikey.
 *
 *   ./telex < keys.txt
 *   echo 'tieengs vieetj' | ./telex
 */
#include "telex.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static void usage(void)
{
	fprintf(stderr, "usage: telex [-m] < keys\n"
		"  -m  modern tone style: hoà/khoẻ/thuỷ instead of hòa/khỏe/thủy\n");
}

int main(int argc, char **argv)
{
	TelexOptions opt = { 0 };
	Telex t;
	char out[TELEX_MAX_OUT];
	int ch;

	opt.spell_check = 1;
	opt.w_at_begin = 1;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-m")) {
			opt.modern_style = 1;
		} else if (!strcmp(argv[i], "-h")) {
			usage();
			return 0;
		} else {
			usage();
			return 2;
		}
	}

	telex_init(&t, &opt);
	while ((ch = getchar()) != EOF) {
		if (isalpha((unsigned char)ch)) {
			telex_key(&t, ch, out, sizeof out, NULL);
		} else if (ch == '\b' || ch == 0x7f) {
			telex_backspace(&t, out, sizeof out, NULL);
		} else {
			int n = telex_flush(&t, out, sizeof out);
			if (n > 0)
				fwrite(out, 1, (size_t)n, stdout);
			putchar(ch);
		}
	}
	int n = telex_flush(&t, out, sizeof out);
	if (n > 0)
		fwrite(out, 1, (size_t)n, stdout);
	return 0;
}
