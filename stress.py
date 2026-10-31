#!/usr/bin/env python3
"""Phonotactic stress test for telex.c.

Builds every syllable the Vietnamese spelling allows (onset x rhyme x tone),
keeps the 10 000 longest, and checks that each one converts from every key
sequence that spells it: free modifier order, free tone position, and
uppercase.  Nothing here is a spelling dictionary; "valid" only means the
shape is phonotactically possible, the word may be meaningless.

    make stress
"""
import collections
import ctypes
import os
import random
import subprocess
import sys
import tempfile
import unicodedata

HERE = os.path.dirname(os.path.abspath(__file__))
CAP = 10000
SEED = 11

ONSETS = {
    '':   [],
    'b':  [('b', None)], 'c': [('c', None)], 'ch': [('c', None), ('h', None)],
    'd':  [('d', None)], 'đ': [('d', 'dd')], 'g': [('g', None)],
    'gh': [('g', None), ('h', None)], 'gi': [('g', None), ('i', None)],
    'h':  [('h', None)], 'k': [('k', None)], 'kh': [('k', None), ('h', None)],
    'l':  [('l', None)], 'm': [('m', None)], 'n': [('n', None)],
    'ng': [('n', None), ('g', None)],
    'ngh': [('n', None), ('g', None), ('h', None)],
    'nh': [('n', None), ('h', None)], 'ph': [('p', None), ('h', None)],
    'qu': [('q', None), ('u', None)], 'r': [('r', None)], 's': [('s', None)],
    't':  [('t', None)], 'th': [('t', None), ('h', None)],
    'tr': [('t', None), ('r', None)], 'v': [('v', None)], 'x': [('x', None)],
}

MARK = {'ă': ('a', 'breve'), 'â': ('a', 'circ'), 'ê': ('e', 'circ'),
        'ô': ('o', 'circ'), 'ơ': ('o', 'horn'), 'ư': ('u', 'horn'),
        'đ': ('d', 'dd')}

# Open rhymes and rhymes with a coda.  Curated: every entry is a shape the
# Vietnamese spelling system allows, whether or not a word uses it.
RHYMES = """
a e ê i o ô ơ u ư y
ai ao au ay âu ây eo êu ia iê iu iêu yêu
oa oe oi ôi ơi oai oao oay oeo ua uâ uê ui uô uy uya uyê uyu uây uôi uơ
ưa ưi ưu ươ ươi ươu
ac ach am an ang anh ap at
ăc ăm ăn ăng ăp ăt
âc âm ân âng âp ât
ec em en ep et êch êm ên ênh êp êt
ich im in inh ip it
oc om on ong op ot ôc ôm ôn ông ôp ôt ơm ơn ơp ơt
uc um un ung up ut ưc ưng ưt
iêc iêm iên iêng iêp iêt yên
uôc uôm uôn uông uôt ươc ươm ươn ương ươp ươt
uân uât uêch uênh oan oang oanh oat oach
oăc oăm oăn oăng oăt uyên uyêt uynh uych uyt oen oet
""".split()

TONES = ['', 's', 'f', 'r', 'x', 'j']
STOP = ('c', 'ch', 'p', 't')


def toks(text):
    return [MARK.get(ch, (ch, None)) for ch in text]


def rhyme_is_stop(text):
    return text.endswith(STOP)


def onset_ok(onset, rhyme):
    front = rhyme[0] in 'ieêy'
    if onset in ('c', 'g', 'ng') and front:
        return False
    if onset in ('k', 'gh', 'ngh') and not front:
        return False
    if onset == 'gi' and rhyme[0] in 'iy':
        return False
    if onset == 'qu' and rhyme[0] in 'uư':
        return False
    return True


def tone_slot(nuc, coda):
    """Classical (Unikey default) tone placement."""
    marked = [i for i, (b, m) in enumerate(nuc) if m]
    if marked:
        return marked[-1]
    if len(nuc) == 1:
        return 0
    if len(nuc) == 3:
        return 1
    last = nuc[-1][0]
    if nuc[0][0] == 'u' and last == 'y':      # thủy / huỳnh, quýt
        return 1 if coda else 0
    if last in 'iyou':
        return 0
    return 1 if coda else 0


def render(onset_t, nuc, coda_t, tone):
    letters = list(onset_t) + list(nuc) + list(coda_t)
    ti = len(onset_t) + tone_slot(nuc, coda_t)
    out = []
    for i, (b, m) in enumerate(letters):
        s = b
        if m == 'breve':
            s = 'ă'
        elif m == 'circ':
            s = {'a': 'â', 'e': 'ê', 'o': 'ô'}[b]
        elif m == 'horn':
            s = {'o': 'ơ', 'u': 'ư'}[b]
        elif m == 'dd':
            s = 'đ'
        if tone and i == ti:
            s += {'s': '\u0301', 'f': '\u0300', 'r': '\u0309',
                  'x': '\u0303', 'j': '\u0323'}[tone]
        out.append(s)
    return unicodedata.normalize('NFC', ''.join(out))


def key_for(base, mark):
    if mark in ('breve', 'horn'):
        return base + 'w'
    if mark == 'circ':
        return base + base
    if mark == 'dd':
        return base + 'd'
    return base


def build(base, mods, tone, slots, tone_at):
    buckets = [[] for _ in range(len(base) + 1)]
    for (key, _), slot in zip(mods, slots):
        buckets[slot].append(key)
    if tone:
        buckets[tone_at].append(tone)
    out = []
    for i in range(len(base) + 1):
        out.extend(buckets[i])
        if i < len(base):
            out.append(base[i])
    return ''.join(out)


def variants(ot, nuc, coda, tone, rnd):
    base = [b for b, _ in ot + nuc + coda]
    mods = []
    for i, (b, m) in enumerate(ot + nuc + coda, 1):
        if m in ('breve', 'horn'):
            mods.append(('w', i))
        elif m == 'circ':
            mods.append((b, i))
        elif m == 'dd':
            mods.append(('d', i))
    v0, L = len(ot), len(base)
    out = {''.join(key_for(b, m) for b, m in ot + nuc + coda) + tone}
    for slot in range(v0 + 1, L + 1):        # tone anywhere after the vowel
        out.add(build(base, mods, tone, [m for _, m in mods], slot))
    for i in range(len(mods)):               # one modifier moved to the end
        slots = [m for _, m in mods]
        slots[i] = L
        out.add(build(base, mods, tone, slots, L if tone else 0))
    out.add(build(base, mods, tone, [L] * len(mods), L if tone else 0))
    for _ in range(12):                      # random free order
        slots = [rnd.randint(m, L) for _, m in mods]
        out.add(build(base, mods, tone, slots,
                      rnd.randint(v0 + 1, L) if tone else 0))
    return out


def load_engine():
    tmp = tempfile.mkdtemp(prefix='telex-stress-')
    so = os.path.join(tmp, 'libtelex.so')
    subprocess.run([os.environ.get('CC', 'cc'), '-O2', '-std=c11', '-fPIC',
                    '-shared', '-o', so, os.path.join(HERE, 'telex.c')],
                   check=True)
    lib = ctypes.CDLL(so)

    class Opt(ctypes.Structure):
        _fields_ = [('modern', ctypes.c_int), ('spell', ctypes.c_int),
                    ('restore', ctypes.c_int), ('w_begin', ctypes.c_int)]

    lib.telex_convert.argtypes = [ctypes.c_char_p, ctypes.c_int,
                                  ctypes.c_void_p, ctypes.c_char_p,
                                  ctypes.c_size_t, ctypes.POINTER(ctypes.c_int)]
    opt = Opt(0, 1, 0, 1)
    buf = ctypes.create_string_buffer(1024)

    def conv(keys):
        lib.telex_convert(keys.encode(), len(keys), ctypes.byref(opt), buf,
                          len(buf), None)
        return buf.value.decode('utf-8', 'replace')
    return conv


def main():
    rnd = random.Random(SEED)
    conv = load_engine()

    words = []
    for onset, ot in ONSETS.items():
        for rh in RHYMES:
            if not onset_ok(onset, rh):
                continue
            rt = toks(rh)
            k = 0
            while k < len(rt) and rt[k][0] in 'aeiouy':
                k += 1
            nuc, coda = rt[:k], rt[k:]
            for tone in (['s', 'j'] if rhyme_is_stop(rh) else TONES):
                words.append((onset, ot, nuc, coda, tone))
    total = len(words)
    words.sort(key=lambda w: -(len(w[0]) + len(w[2]) + len(w[3])))
    words = words[:CAP]

    # canonical result of each word; a key string may legitimately belong to
    # more than one word, and then only "one of them" can be checked
    canon, targets, owners = [], [], collections.defaultdict(set)
    for idx, (onset, ot, nuc, coda, tone) in enumerate(words):
        keys = ''.join(key_for(b, m) for b, m in ot + nuc + coda) + tone
        canon.append(keys)
        targets.append(conv(keys))
        owners[keys].add(idx)

    bad = 0
    checked = 0
    for idx, (onset, ot, nuc, coda, tone) in enumerate(words):
        want = targets[idx]
        keys = set(variants(ot, nuc, coda, tone, rnd))
        keys.add(canon[idx].upper())
        for k in keys:
            checked += 1
            exp = want.upper() if k.isupper() else want
            got = conv(k)
            if got != exp:
                bad += 1
                if bad <= 10:
                    print('FAIL %-12s keys %-14s got %-12s want %s' %
                          (want, k, got, exp))

    # the word itself must match the classical spelling of its shape
    shape = 0
    for idx, (onset, ot, nuc, coda, tone) in enumerate(words):
        want = render(ot, nuc, coda, tone)
        if targets[idx] != want:
            shape += 1
            if shape <= 10:
                print('SHAPE %-12s got %s' % (want, targets[idx]))

    ambiguous = sum(1 for v in owners.values() if len(v) > 1)
    print('shapes %d/%d  keys %d  ambiguous %d  key failures %d  shape '
          'failures %d' % (len(words), total, checked, ambiguous, bad, shape))
    return 1 if bad or shape else 0


if __name__ == '__main__':
    sys.exit(main())
