"""Annotate Turbo Pascal inline(...) statements with their disassembly.

Usage:
    python pasinline.py SOURCE.PAS               annotated source to stdout
    python pasinline.py SOURCE.PAS -o OUT.PAS    annotated source to a file
    python pasinline.py SOURCE.PAS -i            rewrite SOURCE.PAS in place
                                                 (the original goes to .BAK)
    python pasinline.py SOURCE.PAS --strip ...   only remove the annotations

Every instruction gets its own comment {... ;} at the end of the line where
its first byte stands, so the text between the braces assembles as it is.
The trailing ' ;' (an empty assembler comment) is what marks the comment as
ours: running the tool again replaces only those, never a comment of the
author's, so it is safe to repeat after editing the bytes.

Element sizes follow Turbo Pascal 3: a constant is one byte when its value
is 0..255 and a word otherwise, '<' forces a byte and '>' a word, and a
variable or procedure name (or the location counter '*') is a word -- its
address. Names are printed as they stand, so 'LD HL,addr' comes out for
$21/addr. Relative jumps are printed as '$+n', counted from the address of
the jump itself.

The file is handled as raw bytes (latin-1 in, latin-1 out): Kamenicky
letters, CR-only line ends and ^Z padding come back unchanged.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from z180dis import dis

MARK_END = ' ;}'     # closes an annotation and tells it from the author's
RELATIVE = {0x10, 0x18, 0x20, 0x28, 0x30, 0x38}   # DJNZ, JR, JR cc


def scan(text):
    """Yield (kind, start, end) for comments, strings and words; stops at ^Z."""
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == '\x1a':
            return
        if c == '{':
            j = text.find('}', i + 1)
            j = n if j < 0 else j + 1
            yield 'comment', i, j
            i = j
        elif text.startswith('(*', i):
            j = text.find('*)', i + 2)
            j = n if j < 0 else j + 2
            yield 'comment', i, j
            i = j
        elif c == "'":
            j = i + 1
            while j < n:
                if text[j] == "'":
                    if j + 1 < n and text[j + 1] == "'":
                        j += 2
                        continue
                    break
                j += 1
            yield 'string', i, min(j + 1, n)
            i = j + 1
        elif c.isalpha() or c == '_':
            j = i
            while j < n and (text[j].isalnum() or text[j] == '_'):
                j += 1
            yield 'word', i, j
            i = j
        else:
            yield 'char', i, i + 1
            i += 1


def find_inlines(text):
    """Return (open_paren, close_paren) of every inline statement."""
    found = []
    tokens = list(scan(text))
    k = 0
    while k < len(tokens):
        kind, s, e = tokens[k]
        if kind == 'word' and text[s:e].lower() == 'inline':
            m = k + 1
            while m < len(tokens) and tokens[m][0] == 'char' and text[tokens[m][1]].isspace():
                m += 1
            if m < len(tokens) and tokens[m][0] == 'char' and text[tokens[m][1]] == '(':
                depth, q = 0, m
                while q < len(tokens):
                    kq, sq, _ = tokens[q]
                    if kq == 'char' and text[sq] == '(':
                        depth += 1
                    elif kq == 'char' and text[sq] == ')':
                        depth -= 1
                        if depth == 0:
                            found.append((tokens[m][1], sq))
                            k = q
                            break
                    q += 1
        k += 1
    return found


def strip_annotations(text):
    """Remove {... ;} comments (and the blank before them) inside inline()."""
    out, last = [], 0
    for lo, hi in find_inlines(text):
        # The last instruction's comment stands after ');' on the same line.
        hi = line_end(text, hi)
        for kind, s, e in scan(text[lo:hi]):
            body = text[lo + s:lo + e]
            if kind == 'comment' and body.startswith('{') and body.endswith(MARK_END):
                a = lo + s
                while a > last and text[a - 1] in ' \t':
                    a -= 1
                out.append(text[last:a])
                last = lo + e
    out.append(text[last:])
    return ''.join(out)


def parse_number(t):
    t = t.strip()
    if t.startswith('$'):
        return int(t[1:], 16)
    if t.isdigit():
        return int(t)
    if len(t) == 3 and t[0] == t[2] == "'":
        return ord(t[1])
    return None


def elements(text, lo, hi):
    """Split the inside of inline( ) at '/' into (source_offset, element_text)."""
    parts, cur, start = [], [], None
    for kind, s, e in scan(text[lo + 1:hi]):
        piece = text[lo + 1 + s:lo + 1 + e]
        if kind == 'comment':
            continue
        if kind == 'char' and piece == '/':
            parts.append((start, ''.join(cur)))
            cur, start = [], None
            continue
        if kind == 'char' and piece.isspace():
            continue
        if start is None:
            start = lo + 1 + s
        cur.append(piece)
    if start is not None or cur:
        parts.append((start, ''.join(cur)))
    return parts


def assemble(parts):
    """Turn elements into bytes. Names become numbered placeholder words.

    Returns (bytes, owner, symbols): owner[i] is the source offset of the
    element byte i came from, symbols maps byte index -> (name, size).
    """
    data, owner, symbols = [], [], {}
    for where, t in parts:
        if t == '':
            raise ValueError(f'empty element at offset {where}')
        force = None
        if t[0] in '<>':
            force, t = t[0], t[1:]
        value = parse_number(t)
        if value is not None:
            size = 1 if (force == '<' or (force is None and 0 <= value <= 255)) else 2
            if size == 1:
                data.append(value & 0xff)
            else:
                data += [value & 0xff, (value >> 8) & 0xff]
        else:
            size = 1 if force == '<' else 2
            symbols[len(data)] = (t, size)
            data += [0] * size
        owner += [where] * size
    return data, owner, symbols


HEX = re.compile(r'\b([0-9A-F]+)h\b')


def fix_hex(s):
    # 'FE00h' reads as a name to most assemblers; '0FE00h' does not.
    return HEX.sub(lambda m: ('0' if m.group(1)[0] in 'ABCDEF' else '') + m.group(1) + 'h', s)


def disassemble(data, symbols):
    """Yield (offset, length, text) for each instruction."""
    padded = data + [0] * 4
    p = 0
    while p < len(data):
        if p in symbols:
            # A name where an opcode belongs: its value is unknown here, so
            # any mnemonic made from the placeholder would be invented.
            name, size = symbols[p]
            yield p, size, f'{"db" if size == 1 else "dw"} {name}'
            p += size
            continue
        # Fill every placeholder with a value that prints uniquely.
        buf = list(padded)
        marks = {}
        for idx, (name, size) in symbols.items():
            if size == 2:
                v = 0xA000 + idx
                buf[idx], buf[idx + 1] = v & 0xff, v >> 8
                marks[f'{v:04X}h'] = name
            else:
                v = 0xA0 + (idx & 0x1f)
                buf[idx] = v
                marks[f'{v:02X}h'] = name
        s, n = dis(buf, p)
        o = data[p]
        if o in RELATIVE:
            d = data[p + 1] if p + 1 < len(data) else 0
            d = d - 256 if d > 127 else d
            cut = max(s.rfind(' '), s.rfind(','))
            s = f'{s[:cut + 1]}${d + 2:+d}'
        used = [i for i in symbols if p <= i < p + n]
        s = fix_hex(s)
        for i in used:
            name, size = symbols[i]
            v = 0xA000 + i if size == 2 else 0xA0 + (i & 0x1f)
            txt = f'{v:04X}h' if size == 2 else f'{v:02X}h'
            # The longer, zero-prefixed form first, or its tail would match.
            if fix_hex(txt) in s:
                s = s.replace(fix_hex(txt), name)
            elif txt in s:
                s = s.replace(txt, name)
            else:
                s += f' [{name}]'
        if p + n > len(data):
            s += ' (incomplete)'
        yield p, n, s
        p += n


def line_end(text, pos):
    j = pos
    while j < len(text) and text[j] not in '\r\n\x1a':
        j += 1
    return j


def annotate(text):
    text = strip_annotations(text)
    inserts = {}       # line-end position -> list of instruction texts
    for lo, hi in find_inlines(text):
        parts = elements(text, lo, hi)
        if not parts:
            continue
        data, owner, symbols = assemble(parts)
        # Positions inside comments of this statement must not get a comment.
        inside = [(lo + 1 + s, lo + 1 + e) for k, s, e in scan(text[lo + 1:hi]) if k == 'comment']
        for p, n, s in disassemble(data, symbols):
            pos = line_end(text, owner[p])
            while any(a <= pos < b for a, b in inside):
                pos = line_end(text, next(b for a, b in inside if a <= pos < b))
            inserts.setdefault(pos, []).append(s)
    out, last = [], 0
    for pos in sorted(inserts):
        out.append(text[last:pos])
        out.append(''.join(' {' + s + MARK_END for s in inserts[pos]))
        last = pos
    out.append(text[last:])
    return ''.join(out)


def main(argv):
    args = [a for a in argv if not a.startswith('-')]
    flags = [a for a in argv if a.startswith('-')]
    if len(args) < 1:
        print(__doc__)
        return 1
    src = args[0]
    with open(src, 'rb') as f:
        text = f.read().decode('latin-1')
    result = strip_annotations(text) if '--strip' in flags else annotate(text)
    raw = result.encode('latin-1')
    if '-i' in flags:
        with open(src + '.BAK' if '.' not in os.path.basename(src)
                  else os.path.splitext(src)[0] + '.BAK', 'wb') as f:
            f.write(text.encode('latin-1'))
        with open(src, 'wb') as f:
            f.write(raw)
    elif '-o' in flags:
        out = argv[argv.index('-o') + 1]
        with open(out, 'wb') as f:
            f.write(raw)
    else:
        sys.stdout.buffer.write(raw)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
