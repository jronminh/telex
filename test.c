/*
 * Tests for the Telex engine.  Run with `make check`.
 */
#include "telex.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int failures;

static TelexOptions options(int modern)
{
	TelexOptions opt = { 0 };
	opt.spell_check = 1;
	opt.w_at_begin = 1;
	opt.modern_style = modern;
	return opt;
}

static void check_word(const char *keys, const char *want, int modern)
{
	TelexOptions opt = options(modern);
	char out[TELEX_MAX_OUT];
	int n = telex_convert(keys, (int)strlen(keys), &opt, out, sizeof out,
			     NULL);

	if (n < 0 || strcmp(out, want)) {
		printf("FAIL  %-16s -> %-16s (want %s)\n", keys, out, want);
		failures++;
	} else {
		printf("ok    %-16s -> %s\n", keys, out);
	}
}

static int line(const char *keys, char *out, size_t outsz, int modern)
{
	TelexOptions opt = options(modern);
	Telex t;
	size_t k = 0;
	char word[TELEX_MAX_OUT];

	telex_init(&t, &opt);
	for (const char *p = keys; *p; p++) {
		int ch = (unsigned char)*p;
		if (isalpha(ch)) {
			telex_key(&t, ch, word, sizeof word, NULL);
		} else {
			int n = telex_flush(&t, word, sizeof word);
			if (k + (size_t)n + 1 >= outsz)
				return -1;
			memcpy(out + k, word, (size_t)n);
			k += (size_t)n;
			if (k + 1 >= outsz)
				return -1;
			out[k++] = (char)ch;
		}
	}
	int n = telex_flush(&t, word, sizeof word);
	if (k + (size_t)n + 1 >= outsz)
		return -1;
	memcpy(out + k, word, (size_t)n);
	k += (size_t)n;
	out[k] = '\0';
	return 0;
}

static void check_line(const char *keys, const char *want, int modern)
{
	char out[TELEX_MAX_OUT];

	if (line(keys, out, sizeof out, modern) < 0 || strcmp(out, want)) {
		printf("FAIL  %-24s -> %-24s (want %s)\n", keys, out, want);
		failures++;
	} else {
		printf("ok    %-24s -> %s\n", keys, out);
	}
}

static void check_api(void)
{
	TelexOptions opt = options(0);
	Telex t;
	char out[TELEX_MAX_OUT];
	int marked = -1;
	const char *keys = "tieengs";

	telex_init(&t, &opt);
	for (const char *p = keys; *p; p++)
		telex_key(&t, *p, out, sizeof out, &marked);
	if (strcmp(out, "tiếng") || marked != 1) {
		printf("FAIL  api after tieengs: %s (marked %d)\n", out, marked);
		failures++;
	} else {
		printf("ok    api after tieengs -> %s\n", out);
	}

	telex_backspace(&t, out, sizeof out, &marked);
	if (strcmp(out, "tiêng") || marked != 1) {
		printf("FAIL  api backspace -> %s (marked %d)\n", out, marked);
		failures++;
	} else {
		printf("ok    api backspace -> %s\n", out);
	}

	int n = telex_flush(&t, out, sizeof out);
	if (n <= 0 || strcmp(out, "tiêng")) {
		printf("FAIL  api flush -> %s\n", out);
		failures++;
	} else {
		printf("ok    api flush -> %s\n", out);
	}

	if (telex_backspace(&t, out, sizeof out, NULL) != 0 ||
	    telex_key(&t, ' ', out, sizeof out, NULL) != 0) {
		printf("FAIL  api empty backspace / non-letter\n");
		failures++;
	} else {
		printf("ok    api empty backspace / non-letter\n");
	}

	TelexOptions raw = options(0);
	raw.auto_restore = 1;
	char rawo[TELEX_MAX_OUT];
	if (telex_convert("fast", 4, &raw, rawo, sizeof rawo, NULL) < 0 ||
	    strcmp(rawo, "fast")) {
		printf("FAIL  api auto_restore -> %s\n", rawo);
		failures++;
	} else {
		printf("ok    api auto_restore -> %s\n", rawo);
	}

	char tiny[2];
	if (telex_convert("tieengs", 7, &opt, tiny, sizeof tiny, NULL) != -1) {
		printf("FAIL  api small output buffer\n");
		failures++;
	} else {
		printf("ok    api small output buffer\n");
	}
}

int main(void)
{
	/* basic marks */
	check_word("tieengs", "tiếng", 0);
	check_word("vieetj", "việt", 0);
	check_word("dduowngf", "đường", 0);
	check_word("dduwowngf", "đường", 0);
	check_word("truowngf", "trường", 0);
	check_word("nguowif", "người", 0);
	check_word("muoons", "muốn", 0);
	check_word("Tooi", "Tôi", 0);
	check_word("hocj", "học", 0);
	check_word("ddi", "đi", 0);
	check_word("DD", "Đ", 0);
	check_word("DDi", "Đi", 0);
	check_word("Tieengs", "Tiếng", 0);
	check_word("TIEENGS", "TIẾNG", 0);

	/* vowels and tones */
	check_word("aas", "ấ", 0);
	check_word("aaf", "ầ", 0);
	check_word("awf", "ằ", 0);
	check_word("uwf", "ừ", 0);
	check_word("owf", "ờ", 0);
	check_word("as", "á", 0);
	check_word("ass", "as", 0);
	check_word("caf", "cà", 0);
	check_word("cacs", "các", 0);
	check_word("cacf", "càc", 0);
	check_word("quas", "quá", 0);
	check_word("quyeenr", "quyển", 0);
	check_word("nguyeenx", "nguyễn", 0);
	check_word("gif", "gì", 0);
	check_word("giaf", "già", 0);
	check_word("theem", "thêm", 0);
	check_word("tuooir", "tuổi", 0);

	/* more nuclei, codas, qu/gi */
	check_word("quow", "quơ", 0);
	check_word("quowf", "quờ", 0);
	check_word("gios", "gió", 0);
	check_word("giuwx", "giữ", 0);
	check_word("gians", "gián", 0);
	check_word("quynhf", "quỳnh", 0);
	check_word("huynhf", "huỳnh", 0);
	check_word("hoanhs", "hoánh", 0);
	check_word("huychs", "huých", 0);
	check_word("khuyur", "khuỷu", 0);
	check_word("xoays", "xoáy", 0);
	check_word("ngoeor", "ngoẻo", 0);
	check_word("khuaays", "khuấy", 0);
	check_word("hoawcj", "hoặc", 0);
	check_word("xoawn", "xoăn", 0);
	check_word("tuaanf", "tuần", 0);
	check_word("hueej", "huệ", 0);
	check_word("quyeets", "quyết", 0);
	check_word("huyeenj", "huyện", 0);
	check_word("tuyeets", "tuyết", 0);
	check_word("yeeu", "yêu", 0);
	check_word("yeeus", "yếu", 0);
	check_word("tieecs", "tiếc", 0);
	check_word("tieenf", "tiền", 0);
	check_word("muoon", "muôn", 0);
	check_word("muoong", "muông", 0);
	check_word("duowng", "dương", 0);
	check_word("muwa", "mưa", 0);
	check_word("huwu", "hưu", 0);
	check_word("guwir", "gửi", 0);
	check_word("toois", "tối", 0);
	check_word("chowi", "chơi", 0);
	check_word("ddaay", "đây", 0);
	check_word("ddaau", "đâu", 0);
	check_word("meof", "mèo", 0);
	check_word("keeu", "kêu", 0);
	check_word("mias", "mía", 0);
	check_word("chiuj", "chịu", 0);
	check_word("muas", "múa", 0);
	check_word("muis", "múi", 0);
	check_word("thuys", "thúy", 0);
	check_word("hoangf", "hoàng", 0);
	check_word("nghixa", "nghĩa", 0);
	check_word("chuyeenj", "chuyện", 0);
	check_word("quyeens", "quyến", 0);
	check_word("tuowif", "tười", 0);
	check_word("ruowuj", "rượu", 0);
	check_word("tieeuf", "tiều", 0);
	check_word("muoosn", "muốn", 0);
	check_word("aff", "af", 0);
	check_word("asx", "ã", 0);
	check_word("asz", "a", 0);
	check_word("aasz", "â", 0);
	check_word("w", "ư", 0);
	check_word("W", "Ư", 0);
	check_word("wa", "ưa", 0);

	/* uơ vs ươ, and literal oo in loanwords */
	check_word("thuowr", "thuở", 0);
	check_word("huow", "huơ", 0);
	check_word("thuowngf", "thường", 0);
	check_word("thuowng", "thương", 0);
	check_word("dduowngf", "đường", 0);
	check_word("booong", "boong", 0);
	check_word("xooong", "xoong", 0);
	check_word("sooong", "soong", 0);
	check_word("chooongf", "chòong", 0);
	check_word("gooongf", "gòong", 0);
	check_word("dduowcj", "được", 0);
	check_word("khongo", "không", 0);
	check_word("khoong", "không", 0);
	check_word("khongof", "khồng", 0);
	check_word("duongwf", "dường", 0);
	check_word("thuongw", "thương", 0);
	check_word("khongw", "khơng", 0);
	check_word("banw", "băn", 0);
	check_word("hoao", "hoao", 0);
	check_word("goiwj", "gợi", 0);
	check_word("goiwr", "gởi", 0);
	check_word("goijw", "gợi", 0);
	check_word("goiw", "gơi", 0);
	check_word("duwodjc", "được", 0);
	check_word("duwocj", "dược", 0);
	check_word("dduowcj", "được", 0);
	check_word("duowcj", "dược", 0);
	check_word("uwo", "ươ", 0);
	check_word("tuwoi", "tươi", 0);
	check_word("duwongf", "dường", 0);
	check_word("dad", "đa", 0);
	check_word("duwocdj", "được", 0);
	check_word("duwocjd", "được", 0);
	check_word("duwjodc", "được", 0);
	check_word("khongfo", "khồng", 0);
	check_word("goiwjr", "gởi", 0);
	check_word("hoanw", "hoăn", 0);
	check_word("dadj", "đạ", 0);
	check_word("goiwjys", "gớiy", 0);
	check_word("khoongo", "khongo", 0);
	check_word("boongo", "bongo", 0);
	check_word("khoong", "không", 0);
	check_word("khongo", "không", 0);
	check_word("uww", "uw", 0);
	check_word("aww", "aw", 0);
	check_word("oww", "ow", 0);
	check_word("aaa", "aa", 0);
	check_word("ooo", "oo", 0);
	check_word("ddd", "dd", 0);
	check_word("duowng", "dương", 0);
	check_word("duongw", "dương", 0);
	check_word("duwong", "dương", 0);
	check_word("duongwf", "dường", 0);
	check_word("quyeen", "quyên", 0);
	check_word("quyene", "quyên", 0);
	check_word("quyenef", "quyền", 0);
	check_word("dduowmj", "đượm", 0);
	check_word("duwomjd", "đượm", 0);
	check_word("dduowmjg", "đượmg", 0);
	check_word("af", "à", 0);
	check_word("uf", "ù", 0);
	check_word("uwf", "ừ", 0);
	check_word("us", "ú", 0);
	/* free-order stress cases found by fuzzing */
	check_word("phuaws", "phứa", 0);
	check_word("choaos", "choáo", 0);
	check_word("choaof", "choào", 0);
	check_word("doaod", "đoao", 0);
	check_word("luoxiww", "lưỡi", 0);
	check_word("diender", "điển", 0);
	check_word("buowuws", "bướu", 0);
	check_word("duorido", "đuổi", 0);
	check_word("chuyus", "chuýu", 0);
	check_word("suwowuf", "sườu", 0);
	check_word("giees", "giế", 0);
	check_word("ruychs", "ruých", 0);
	check_word("myns", "mýn", 0);
	check_word("gieengs", "giếng", 0);
	check_word("AS", "Á", 0);

	/* tone style */
	check_word("hoaf", "hòa", 0);
	check_word("hoaf", "hoà", 1);
	check_word("khoer", "khỏe", 0);
	check_word("khoer", "khoẻ", 1);
	check_word("thuyr", "thủy", 0);
	check_word("thuyr", "thuỷ", 1);
	check_word("quyr", "quỷ", 0);

	/* English that is not a Vietnamese shape */
	check_word("hello", "hello", 0);
	check_word("fast", "fát", 0);
	check_word("book", "bôk", 0);
	check_word("world", "ưởld", 0);

	/* English that happens to be a valid Vietnamese syllable */
	check_word("the", "the", 0);
	check_word("test", "tét", 0);
	check_word("this", "thí", 0);

	/* whole lines */
	check_line("Tooi muoons ddi hocj", "Tôi muốn đi học", 0);
	check_line("tieengs vieetj", "tiếng việt", 0);
	check_line("goiwj ys", "gợi ý", 0);
	check_line("khong o", "khong o", 0);
	check_line("af us", "à ú", 0);
	check_line("af uwf", "à ừ", 0);

	check_api();

	if (failures) {
		printf("\n%d failure(s)\n", failures);
		return 1;
	}
	printf("\nall tests passed\n");
	return 0;
}
