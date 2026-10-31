# telex

A small, pure-C Telex engine for a US keyboard. It has no Wayland or
input-method code: it turns letters into the current Vietnamese word, and the
caller decides how to deliver that word.

```sh
make check
echo 'tieengs vieetj dduowngf' | ./telex
# tiếng việt đường
./telex -m < keys.txt          # hoà/khoẻ/thuỷ tone style
```

## API

```c
Telex t;
telex_init(&t, NULL);          // traditional style, spell check on
telex_key(&t, 't', out, sizeof out, &marked);
telex_key(&t, 'i', out, sizeof out, &marked);
...
telex_flush(&t, out, sizeof out);   // space, punctuation, focus loss
```

`telex_key()` returns the current word: the composed Vietnamese form, or the
raw keys if it is not a plausible syllable. `marked` says whether the result
contains a Vietnamese mark. `telex_backspace()` edits the current word.

## Rules

- `aa ee oo` → â ê ô
- `aw ow uw` → ă ơ ư; `uow` → ươ
- `dd` → đ
- `s f r x j` → sắc huyền hỏi ngã nặng; `z` removes the tone
- Tone placement follows Vietnamese syllables: a marked vowel wins; `oa`,
  `oe`, `uy` are modern-style options; 2-vowel open words take the first
  vowel, closed words the second; 3-vowel nuclei take the middle.
- `qu` and `gi` are onsets (`quá`, `già`).
- A word that does not look like a Vietnamese syllable is returned as the raw
  US keys (`hello`, `fast`, `book`, `world`).

## Modifier order

A modifier does not have to touch the vowel it completes. The engine looks
back through the syllable for the matching vowel (stopping at another vowel),
so all of these complete the same word:

```text
khoong   không
khongo   không
khongof  khồng
duongwf  dường
thuongw  thương
goiwj    gợi
goiwr    gởi
```

One `w` is enough; it targets the last `a/o/u` in the current vowel group,
even when another vowel follows (`goi` + `w` + `j`). `ww` is not needed.


If applying the modifier would not make a plausible syllable, the key stays
literal: `oeo` is a new vowel, not an `ô` modifier, and `hoao` stays raw.


## Limits

- Telex always converts a plausible syllable. English words that happen to be
  valid Vietnamese syllables still convert: `this` → `thí`, `test` → `tét`.
  The caller needs an English/Vietnamese toggle (the next step in tapkeys).
- No VNI, VIQR, macros, custom keymaps, spelling dictionary, or free-marking
  option; those can come later.
- The final `đ` is accepted as a letter; other lone consonants stay raw.

The algorithm was written with the Unikey engine (Pham Kim Long,
LGPL-2.0-or-later) as a reference; no Unikey code is copied here.

On the 4,525 single words of the Viet22K list, the only mismatches (22–23)
are modern vs traditional tone style — that list mixes both. Everything else
round-trips, including `qu`/`gi`, `uơ`/`ươ`, and the `oo` of `boong`/`xoong`.

