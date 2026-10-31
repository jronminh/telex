/*
 * telex.c — a small Telex engine, pure C, no Wayland.
 *
 * The rules follow the Unikey engine (Pham Kim Long, LGPL-2.0-or-later) as a
 * reference; this is an independent, much smaller implementation.  It is
 * deliberately a word converter: the caller feeds letters and gets the
 * current Vietnamese word back, then decides how to deliver it.
 *
 * Supported:
 *   aa ee oo        â ê ô
 *   aw ow uw        ă ơ ư        (uow also becomes ươ)
 *   dd              đ
 *   s f r x j       sắc huyền hỏi ngã nặng
 *   z               remove the tone
 *   tone placement  one vowel, marked vowel, oa/oe/uy modern style,
 *                   2-vowel open/closed, 3-vowel middle
 *   auto restore    a word that is not a plausible Vietnamese syllable is
 *                   returned as the raw US keys ("hello", "fast", ...)
 *
 * Not supported (on purpose, for now): VNI, VIQR, macros, custom keymaps,
 * spelling dictionary, and the "English word" heuristic.  Telex always
 * converts a plausible syllable, so use an English/Vietnamese toggle when
 * typing English words such as "this" (which is a valid "thí").
 */
#include "telex.h"

#include <ctype.h>
#include <string.h>

/* vowel ids */
enum {
	V_A, V_AB, V_AC,          /* a, ă, â */
	V_E, V_EC,                /* e, ê */
	V_I,
	V_O, V_OC, V_OH,          /* o, ô, ơ */
	V_U, V_UH,                /* u, ư */
	V_Y
};

/* Compose base+mark+tone.  The tones are: none, sắc, huyền, hỏi, ngã, nặng. */
static const char *const VOWEL[12][6][2] = {
    { { "a", "A" }, { "á", "Á" }, { "à", "À" }, { "ả", "Ả" }, { "ã", "Ã" }, { "ạ", "Ạ" } },
    { { "ă", "Ă" }, { "ắ", "Ắ" }, { "ằ", "Ằ" }, { "ẳ", "Ẳ" }, { "ẵ", "Ẵ" }, { "ặ", "Ặ" } },
    { { "â", "Â" }, { "ấ", "Ấ" }, { "ầ", "Ầ" }, { "ẩ", "Ẩ" }, { "ẫ", "Ẫ" }, { "ậ", "Ậ" } },
    { { "e", "E" }, { "é", "É" }, { "è", "È" }, { "ẻ", "Ẻ" }, { "ẽ", "Ẽ" }, { "ẹ", "Ẹ" } },
    { { "ê", "Ê" }, { "ế", "Ế" }, { "ề", "Ề" }, { "ể", "Ể" }, { "ễ", "Ễ" }, { "ệ", "Ệ" } },
    { { "i", "I" }, { "í", "Í" }, { "ì", "Ì" }, { "ỉ", "Ỉ" }, { "ĩ", "Ĩ" }, { "ị", "Ị" } },
    { { "o", "O" }, { "ó", "Ó" }, { "ò", "Ò" }, { "ỏ", "Ỏ" }, { "õ", "Õ" }, { "ọ", "Ọ" } },
    { { "ô", "Ô" }, { "ố", "Ố" }, { "ồ", "Ồ" }, { "ổ", "Ổ" }, { "ỗ", "Ỗ" }, { "ộ", "Ộ" } },
    { { "ơ", "Ơ" }, { "ớ", "Ớ" }, { "ờ", "Ờ" }, { "ở", "Ở" }, { "ỡ", "Ỡ" }, { "ợ", "Ợ" } },
    { { "u", "U" }, { "ú", "Ú" }, { "ù", "Ù" }, { "ủ", "Ủ" }, { "ũ", "Ũ" }, { "ụ", "Ụ" } },
    { { "ư", "Ư" }, { "ứ", "Ứ" }, { "ừ", "Ừ" }, { "ử", "Ử" }, { "ữ", "Ữ" }, { "ự", "Ự" } },
    { { "y", "Y" }, { "ý", "Ý" }, { "ỳ", "Ỳ" }, { "ỷ", "Ỷ" }, { "ỹ", "Ỹ" }, { "ỵ", "Ỵ" } },
};

/* Code used only to compare a nucleus with the table. */
static const char VCODE[12] = { 'a', 'b', 'c', 'e', 'f', 'i',
				'o', 'p', 'q', 'u', 'v', 'y' };

static const char *const ONSETS[] = {
	"", "b", "c", "ch", "d", "D", "g", "gh", "gi", "h", "k", "kh",
	"l", "m", "n", "ng", "ngh", "nh", "p", "ph", "qu", "r", "s", "t",
	"th", "tr", "v", "x", NULL
};

static const char *const CODAS[] = {
	"", "c", "ch", "m", "n", "ng", "nh", "p", "t", NULL
};

/* Plausible Vietnamese vowel nuclei, in VCODE form. */
static const char *const NUCLEI[] = {
	"a", "b", "c", "e", "f", "i", "o", "p", "q", "u", "v", "y",
	"ai", "ao", "au", "ay", "cu", "cy", "eo", "fu", "ia", "if", "iu",
	"oa", "ob", "oe", "oi", "pi", "qi", "ua", "uc", "uf", "ui",
	"up", "uq", "uy", "va", "vi", "vu", "vq", "yf", "oo",
	"ifu", "oai", "oay", "oeo", "ucy", "upi", "uya", "uyf", "uyu",
	"vqi", "vqu", "yfu", NULL
};

typedef struct {
	int vowel;   /* 0..11, or -1 for a consonant */
	int mark;    /* only for đ: 4 */
	char base;   /* lowercase ASCII base */
	int upper;
} Letter;

typedef struct {
	Letter l[TELEX_MAX_KEYS];
	int n;
	int tone;       /* 0..5 */
	int tone_raw;   /* index in raw of the tone key, or -1 */
	int drop[TELEX_MAX_KEYS]; /* keys to omit when restoring raw text */
} Word;

static int is_vowel_base(int c)
{
	return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' ||
	       c == 'y';
}

static int vowel_id(int base, int mark)
{
	switch (base) {
	case 'a':
		return mark == 0 ? V_A : mark == 1 ? V_AB : mark == 2 ? V_AC : -1;
	case 'e':
		return mark == 0 ? V_E : mark == 2 ? V_EC : -1;
	case 'i':
		return mark == 0 ? V_I : -1;
	case 'o':
		return mark == 0 ? V_O : mark == 2 ? V_OC : mark == 3 ? V_OH : -1;
	case 'u':
		return mark == 0 ? V_U : mark == 3 ? V_UH : -1;
	case 'y':
		return mark == 0 ? V_Y : -1;
	}
	return -1;
}

static int is_marked_id(int id)
{
	return id == V_AB || id == V_AC || id == V_EC || id == V_OC ||
	       id == V_OH || id == V_UH;
}

static char vowel_base(int id)
{
	switch (id) {
	case V_A: case V_AB: case V_AC: return 'a';
	case V_E: case V_EC: return 'e';
	case V_I: return 'i';
	case V_O: case V_OC: case V_OH: return 'o';
	case V_U: case V_UH: return 'u';
	case V_Y: return 'y';
	}
	return '?';
}

static int tone_value(int c)
{
	switch (c) {
	case 's': return 1;
	case 'f': return 2;
	case 'r': return 3;
	case 'x': return 4;
	case 'j': return 5;
	case 'z': return 0;
	}
	return -1;
}

static int in_list(const char *const *list, const char *s)
{
	for (int i = 0; list[i]; i++)
		if (!strcmp(list[i], s))
			return 1;
	return 0;
}

static void add_letter(Word *w, int ch)
{
	if (w->n >= TELEX_MAX_KEYS)
		return;
	Letter l;
	l.base = (char)tolower((unsigned char)ch);
	l.upper = isupper((unsigned char)ch) != 0;
	l.mark = 0;
	l.vowel = is_vowel_base(l.base) ? vowel_id(l.base, 0) : -1;
	w->l[w->n++] = l;
}

static void add_vowel(Word *w, int id, int upper)
{
	if (w->n >= TELEX_MAX_KEYS)
		return;
	Letter l;
	l.vowel = id;
	l.mark = 0;
	l.base = vowel_base(id);
	l.upper = upper;
	w->l[w->n++] = l;
}

static int has_vowel(const Word *w)
{
	for (int i = 0; i < w->n; i++)
		if (w->l[i].vowel >= 0)
			return 1;
	return 0;
}

static int analyze(const Word *w, const TelexOptions *opt, int *ns, int *ne,
		   int *tp, int *marked);

/* the last vowel group of the word: [start, end) */
static void vowel_span(const Word *w, int *start, int *end)
{
	int e = w->n;
	while (e > 0 && w->l[e - 1].vowel < 0)
		e--;
	int s = e;
	while (s > 0 && w->l[s - 1].vowel >= 0)
		s--;
	*start = s;
	*end = e;
}

/* the last matching vowel in the vowel group, for a/e/o roofs */
static int roof_target(const Word *w, int base, int marked)
{
	int s, e;
	vowel_span(w, &s, &e);
	for (int i = e - 1; i >= s; i--)
		if (w->l[i].vowel == base || w->l[i].vowel == marked)
			return i;
	return -1;
}

/* the last a/o/u vowel in the vowel group, for the w hook/breve */
static int hook_target(const Word *w)
{
	int s, e;
	vowel_span(w, &s, &e);
	for (int i = e - 1; i >= s; i--) {
		int v = w->l[i].vowel;
		if (v == V_A || v == V_O || v == V_U ||
		    v == V_AB || v == V_OH || v == V_UH)
			return i;
	}
	return -1;
}

/* would the word be a plausible syllable after a trial modifier? */
static int valid_word(const Word *w, const TelexOptions *opt)
{
	int ns, ne, tp, marked;
	return analyze(w, opt, &ns, &ne, &tp, &marked);
}

/*
 * Parse the raw US keys.  This is the whole Telex state machine: a modifier
 * is consumed when it applies, otherwise it is kept as a literal letter.
 * Modifiers may be away from the vowel they complete (Unikey's free
 * marking), e.g. "khongo" and "khoong" both make "không".
 */
static void parse_word(const char *keys, int n, const TelexOptions *opt,
		       Word *w)
{
	memset(w, 0, sizeof *w);
	w->tone_raw = -1;

	for (int i = 0; i < n; i++) {
		int ch = (unsigned char)keys[i];
		int lower = tolower(ch);
		int tone;

		if (!isalpha(ch)) {
			add_letter(w, ch);
			continue;
		}

		tone = tone_value(lower);
		if (tone >= 0) {
			if (tone == 0) {
				/* z: remove a tone, or stay as a literal z */
				if (w->tone) {
					w->tone = 0;
					w->tone_raw = -1;
				} else {
					add_letter(w, ch);
				}
				continue;
			}
			if (has_vowel(w)) {
				if (w->tone == tone) {
					/* same tone twice: undo it and keep one
					 * literal letter, so "ass" -> "as" */
					if (w->tone_raw >= 0)
						w->drop[w->tone_raw] = 1;
					w->tone = 0;
					w->tone_raw = -1;
					add_letter(w, ch);
				} else {
					w->tone = tone;
					w->tone_raw = i;
				}
			} else {
				add_letter(w, ch);
			}
			continue;
		}

		if (lower == 'w') {
			int hi = hook_target(w);
			if (hi >= 0) {
				Letter *target = &w->l[hi];
				int new_id = target->vowel == V_A ? V_AB :
					     target->vowel == V_O ? V_OH :
					     target->vowel == V_U ? V_UH : -1;
				if (new_id < 0)
					continue;   /* already hooked */
				Word trial = *w;
				trial.l[hi].vowel = new_id;
				if (valid_word(&trial, opt)) {
					target->vowel = new_id;
					target->upper |= isupper(ch) != 0;
					continue;
				}
			} else if (w->n == 0 && opt->w_at_begin) {
				add_vowel(w, V_UH, isupper(ch) != 0);
				continue;
			}
			add_letter(w, ch);
			continue;
		}

		if ((lower == 'a' || lower == 'e' || lower == 'o') && w->n > 0) {
			int base_id = lower == 'a' ? V_A :
				      lower == 'e' ? V_E : V_O;
			int circ_id = lower == 'a' ? V_AC :
				      lower == 'e' ? V_EC : V_OC;
			int idx = roof_target(w, base_id, circ_id);
			if (idx >= 0) {
				Letter *target = &w->l[idx];
				if (target->vowel == base_id) {
					Word trial = *w;
					trial.l[idx].vowel = circ_id;
					if (valid_word(&trial, opt)) {
						target->vowel = circ_id;
						target->upper |= isupper(ch) != 0;
						continue;
					}
				} else {
					/* aaa/eee/ooo: undo the mark and add a
					 * literal letter, so "booong" -> "boong" */
					Word trial = *w;
					trial.l[idx].vowel = base_id;
					add_letter(&trial, ch);
					if (valid_word(&trial, opt)) {
						*w = trial;
						continue;
					}
				}
			}
		}

		if (lower == 'd' && w->n > 0) {
			Letter *last = &w->l[w->n - 1];
			if (last->base == 'd' && last->mark == 0 &&
			    last->vowel < 0) {
				last->mark = 4;   /* đ */
				last->upper |= isupper(ch) != 0;
				continue;
			}
		}

		add_letter(w, ch);
	}

	/*
	 * "uow" is normally ươ, but a final uơ exists (thuở, huơ, quơ).
	 * Mark only the o while parsing; if a coda or a third vowel follows,
	 * upgrade the u to ư (thương, người, được).
	 */
	for (int i = 0; i + 1 < w->n; i++) {
		if (w->l[i].vowel == V_U && w->l[i + 1].vowel == V_OH) {
			int qu_onset = i == 1 && w->l[0].base == 'q';
			if (!qu_onset && i + 2 < w->n)
				w->l[i].vowel = V_UH;
		}
	}
}

/*
 * Check the word shape and compute the tone target.  Returns 1 when the
 * letters form a plausible Vietnamese syllable, 0 otherwise.
 */
static int analyze(const Word *w, const TelexOptions *opt, int *nucleus_start,
		   int *nucleus_end, int *tone_pos, int *marked)
{
	int n = w->n, v0 = -1;

	*nucleus_start = *nucleus_end = -1;
	*tone_pos = -1;
	*marked = 0;

	/* a lone đ is a letter, not a syllable; still useful to keep it */
	if (n == 1 && w->l[0].base == 'd' && w->l[0].mark == 4) {
		*marked = 1;
		return 1;
	}

	for (int i = 0; i < n; i++) {
		if (w->l[i].vowel >= 0) {
			v0 = i;
			break;
		}
	}
	if (v0 < 0)
		return 0;

	int start = v0;
	/* qu and gi are onsets: the u/i is not part of the nucleus */
	if (v0 >= 1 && w->l[v0 - 1].base == 'q' && w->l[v0].vowel == V_U &&
	    v0 + 1 < n && w->l[v0 + 1].vowel >= 0)
		start = v0 + 1;
	else if (v0 >= 1 && w->l[v0 - 1].base == 'g' &&
		 w->l[v0].vowel == V_I && v0 + 1 < n &&
		 w->l[v0 + 1].vowel >= 0)
		start = v0 + 1;

	int end = start;
	while (end < n && w->l[end].vowel >= 0)
		end++;

	/* a vowel after the coda means two vowel groups: not a word */
	for (int i = end; i < n; i++)
		if (w->l[i].vowel >= 0)
			return 0;

	if (end - start < 1 || end - start > 3)
		return 0;

	char onset[16] = { 0 }, coda[16] = { 0 }, nucleus[8] = { 0 };
	int oi = 0, ci = 0;
	for (int i = 0; i < start; i++) {
		if (oi >= (int)sizeof onset - 1)
			return 0;
		if (w->l[i].vowel >= 0) {
			/* only the u of qu / i of gi can be here */
			if (!((w->l[i].vowel == V_U && start == v0 + 1) ||
			      (w->l[i].vowel == V_I && start == v0 + 1)))
				return 0;
		}
		onset[oi++] = w->l[i].base == 'd' && w->l[i].mark == 4
				  ? 'D' : w->l[i].base;
	}
	for (int i = end; i < n; i++) {
		if (ci >= (int)sizeof coda - 1)
			return 0;
		coda[ci++] = w->l[i].base == 'd' && w->l[i].mark == 4
				  ? 'D' : w->l[i].base;
	}
	for (int i = start; i < end; i++)
		nucleus[i - start] = VCODE[w->l[i].vowel];

	if (!in_list(ONSETS, onset) || !in_list(CODAS, coda))
		return 0;
	if (opt->spell_check && !in_list(NUCLEI, nucleus))
		return 0;

	int len = end - start;
	int idx = -1;
	for (int i = end - 1; i >= start; i--) {
		if (is_marked_id(w->l[i].vowel)) {
			idx = i - start;
			break;
		}
	}
	if (idx < 0) {
		if (!strcmp(nucleus, "oo")) {
			/* loanword long o: chòong, not choòng */
			idx = 0;
		} else if (len == 1) {
			idx = 0;
		} else if (len == 3) {
			idx = 1;
		} else if (len == 2) {
			int a = w->l[start].vowel;
			int b = w->l[start + 1].vowel;
			int modern = a == V_O && (b == V_A || b == V_E);
			modern = modern || (a == V_U && b == V_Y);
			if (opt->modern_style && modern)
				idx = 1;
			else
				idx = coda[0] ? 1 : 0;
		} else {
			idx = 0;
		}
	}

	/* c/ch/p/t only take sắc and nặng, not huyền/hỏi/ngã */
	if (w->tone >= 2 && w->tone <= 4 &&
	    (!strcmp(coda, "c") || !strcmp(coda, "ch") ||
	     !strcmp(coda, "p") || !strcmp(coda, "t")))
		return 0;

	if (w->tone)
		*marked = 1;
	for (int i = start; i < end; i++)
		if (is_marked_id(w->l[i].vowel))
			*marked = 1;

	*nucleus_start = start;
	*nucleus_end = end;
	*tone_pos = start + idx;
	return 1;
}

static int render(const Word *w, int tone_pos, char *out, size_t outsz)
{
	size_t k = 0;

	for (int i = 0; i < w->n; i++) {
		const char *s;
		char buf[2] = { 0, 0 };

		if (w->l[i].vowel >= 0) {
			int tone = i == tone_pos ? w->tone : 0;
			s = VOWEL[w->l[i].vowel][tone][w->l[i].upper ? 1 : 0];
		} else if (w->l[i].base == 'd' && w->l[i].mark == 4) {
			s = w->l[i].upper ? "Đ" : "đ";
		} else {
			buf[0] = w->l[i].upper
					 ? (char)toupper((unsigned char)w->l[i].base)
					 : w->l[i].base;
			s = buf;
		}
		size_t len = strlen(s);
		if (k + len >= outsz)
			return -1;
		memcpy(out + k, s, len);
		k += len;
	}
	out[k] = '\0';
	return (int)k;
}

int telex_convert(const char *keys, int n, const TelexOptions *opt,
		  char *out, size_t outsz, int *marked)
{
	Word w;
	int ns, ne, tp, m = 0;

	if (outsz == 0 || n < 0 || n > TELEX_MAX_KEYS)
		return -1;
	parse_word(keys, n, opt, &w);
	if (!analyze(&w, opt, &ns, &ne, &tp, &m)) {
		size_t k = 0;
		for (int i = 0; i < n; i++) {
			if (w.drop[i])
				continue;
			if (k + 1 >= outsz)
				return -1;
			out[k++] = keys[i];
		}
		out[k] = '\0';
		if (marked)
			*marked = 0;
		return (int)k;
	}
	int len = render(&w, tp, out, outsz);
	if (len < 0)
		return -1;
	if (marked)
		*marked = m;
	return len;
}

void telex_init(Telex *t, const TelexOptions *opt)
{
	memset(t, 0, sizeof *t);
	if (opt) {
		t->opt = *opt;
	} else {
		t->opt.modern_style = 0;
		t->opt.spell_check = 1;
		t->opt.w_at_begin = 1;
	}
}

void telex_reset(Telex *t)
{
	t->nraw = 0;
}

int telex_key(Telex *t, int ch, char *out, size_t outsz, int *marked)
{
	if (!isalpha((unsigned char)ch))
		return 0;
	if (t->nraw >= TELEX_MAX_KEYS - 1)
		return 0;
	t->raw[t->nraw++] = (char)ch;
	if (telex_convert(t->raw, t->nraw, &t->opt, out, outsz, marked) < 0)
		return 0;
	return 1;
}

int telex_backspace(Telex *t, char *out, size_t outsz, int *marked)
{
	if (t->nraw == 0)
		return 0;
	t->nraw--;
	if (t->nraw == 0) {
		if (outsz)
			out[0] = '\0';
		if (marked)
			*marked = 0;
		return 1;
	}
	if (telex_convert(t->raw, t->nraw, &t->opt, out, outsz, marked) < 0)
		return 0;
	return 1;
}

int telex_flush(Telex *t, char *out, size_t outsz)
{
	int n;
	int marked = 0;

	if (t->nraw == 0) {
		if (outsz)
			out[0] = '\0';
		return 0;
	}
	n = telex_convert(t->raw, t->nraw, &t->opt, out, outsz, &marked);
	t->nraw = 0;
	if (n < 0) {
		if (outsz)
			out[0] = '\0';
		return 0;
	}
	return n;
}
