"""Create an expanded *copy* of a deployed _RES.TXT for roster regression tests.

Never changes the input. Copies the full first Moschops definition, preserving
working animations and spawning, then appends a new characters section so all
original IDs remain stable. Output is synthetic test data, not a distributable mod.
"""
import argparse
from pathlib import Path
import re


def character_blocks(text):
    active = False
    depth = 0
    start = 0
    blocks = []
    for match in re.finditer(r"//[^\r\n]*|'[^']*'|[{}]|\bcharacters\b", text):
        token = match.group()
        if token.startswith(("//", "'")):
            continue
        if not active:
            if token == 'characters':
                active = True
            continue
        if token == '{':
            if depth == 1:
                start = match.start()
            depth += 1
        elif token == '}':
            depth -= 1
            if depth == 1:
                blocks.append(text[start:match.end()])
            elif depth == 0:
                active = False
    if depth:
        raise ValueError('Unbalanced characters section')
    return blocks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--count', type=int, default=513)
    args = parser.parse_args()
    if args.source.resolve() == args.output.resolve() or args.output.exists():
        parser.error('Output must be a new file distinct from the source')
    original = args.source.read_bytes()
    text = original.decode('latin1')
    blocks = character_blocks(text)
    if args.count <= len(blocks):
        parser.error('Requested count must exceed original roster')
    template = next(b for b in blocks if re.search(r"name\s*=\s*'Moschops'", b))
    copies = [re.sub(r"(name\s*=\s*)'Moschops'", rf"\1'Roster probe {i}'", template, count=1)
              for i in range(len(blocks), args.count)]
    # The shipped ambient group selects in order. Randomise only this fixture
    # so a short capture actually loads high IDs rather than the first 8 copies.
    table_start = text.index('spawntable')
    first_group = re.search(r'spawngroup\s*{', text[table_start:])
    if first_group is None:
        raise ValueError('No ambient spawn group')
    insert = table_start + first_group.end()
    test_text = text[:insert] + '\r\n randomise = TRUE\r\n' + text[insert:]
    fixture = (test_text + '\r\ncharacters {\r\n' + '\r\n'.join(copies) + '\r\n}\r\n').encode('latin1')
    assert len(character_blocks(fixture.decode('latin1'))) == args.count
    args.output.write_bytes(fixture)
    print(f'Created {args.count} definitions ({len(blocks)} original); source unchanged')


if __name__ == '__main__':
    main()
