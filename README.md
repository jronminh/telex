# telex

A small, pure-C Telex engine for a US keyboard. It has no Wayland or
input-method code: it turns letters into the current Vietnamese word, and the
caller decides how to deliver that word.

```sh
make check
make stress                    # every phonotactic shape x key orders
echo 'tieengs vieetj dduowngf' | ./telex
# tiếng việt đường
./telex -m < keys.txt          # hoà/khoẻ/thuỷ tone style
```

`make stress` builds every syllable the Vietnamese spelling allows (onset x
rhyme x tone, meaningless words included), keeps the 10 000 longest, and
converts each one from every key sequence that spells it: free modifier
order, free tone position, uppercase. The 58 000 checks must all agree with
the canonical spelling.

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
- **No spell check by default.** Modifiers are still applied to the letters
  they match, but a shape that is not a valid Vietnamese syllable is rendered
  composed instead of being reverted: `khoongo` → `khongo`,
  `dduowmjg` → `đượmg`, `fast` → `fát`, `book` → `bôk`.
- `auto_restore=1` in `TelexOptions` restores the old behaviour: return the
  raw key sequence when the word is not valid.

## Modifier order

A modifier does not have to touch the vowel it completes. The engine keeps
the current word's raw keys until a space/punctuation boundary; any later
valid modifier key can complete an earlier letter in that word, at any
distance. These all complete the same word:

```text
khoong   không
khongo   không
khongof  khồng
duongwf  dường
thuongw  thương
goiwj    gợi
goiwr    gởi
duwodjc  được
duwocdj  được
duwjodc  được
khongfo  khồng
goiwj ys gợi ý
```

A word boundary (space or punctuation) is still required; a modifier after
the boundary does not reach back into the previous word (`khong o` stays
`khong o`).

## Mixed English/Vietnamese

A repeated modifier is the escape for its literal key, and also undoes a mark
you did not want:

```text
uww   uw          aww   aw          oww   ow
aaa   aa          ooo   oo          ddd   dd
ass   as          aff   af
```

So the second `w` must undo the hook and stay a literal `w`, not stay `ư`;
otherwise English words such as `uw`, `aw`, `ow` are impossible to type
without switching the input method off. This is also the editing rule: type
the modifier again to take it back.

An `o` repeated toggles the roof, wherever the roofed vowel is: `booong` →
`boong`, and `khoongo` → `khongo` (the extra key removes the roof *and* stays
literal, so `khongo` is typeable). `khong` and `khoong` both give `không`.

In vnime this is the word the engine is composing; moving the caret with
arrow keys ends it. Mouse-clicking into an older word is not tracked by the
engine (use `preedit=on` if that matters).

The last example is deliberately out of order: `w` hooks the `u`, the `o`
next to the now-hooked `ư` becomes `ơ`, the later `d` completes the onset
`d` to `đ`, and `j` is the tone. A modifier is applied when it can complete
a letter; if it cannot, it stays a literal letter. The final word is rendered
even when it is not a valid syllable (no spell check), unless
`auto_restore=1`.

One `w` is enough; it targets the last `a/o/u` in the current vowel group,
even when another vowel follows (`goi` + `w` + `j`). `ww` is not needed.


If applying the modifier would not make a plausible syllable, the key stays
literal: `oeo` is a new vowel, not an `ô` modifier, and `hoao` stays raw.

When the left-to-right pass cannot make a valid syllable, a small fallback
search tries all readings of the modifier keys together (each `w`, roof, `d`
or tone may complete a different letter, or stay literal) and picks the most
marked valid syllable. This is what handles hard out-of-order inputs such as
`phuaws` → phứa, `luoxiww` → lưỡi, `diender` → điển, `buowuws` → bướu.


## Limits

- There is no spell check: English-like input gets Vietnamese marks when a
  modifier matches (`fast` → `fát`). The caller needs an English/Vietnamese
  toggle; `auto_restore=1` is the raw-key fallback for tools that prefer it.
- No VNI, VIQR, macros, custom keymaps, spelling dictionary, or free-marking
  option; those can come later.
- The final `đ` is accepted as a letter; other lone consonants stay raw.

The algorithm was written with the Unikey engine (Pham Kim Long,
LGPL-2.0-or-later) as a reference; no Unikey code is copied here.

On the 4,525 single words of the Viet22K list, the only mismatches (22–23)
are modern vs traditional tone style — that list mixes both. Everything else
round-trips, including `qu`/`gi`, `uơ`/`ươ`, and the `oo` of `boong`/`xoong`.

