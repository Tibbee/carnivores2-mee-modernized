#!/usr/bin/env python3
"""Comment accounting for the Carnivores 2 ME sources.

Three jobs, all built on one lexer so they cannot disagree with each other:

  stats [REV]          how many comments there are, by kind and by file
  dump  [REV]          the whole corpus as JSON, for grouping and review
  diff  REV_A [REV_B]  what happened to the comments between two revisions,
                       and whether the code itself moved

REV is any git revision. "--" means the working tree and is the default.

Why a lexer rather than grep: a // inside a string literal is not a comment,
and this project has several (URLs, format strings). The scanner below walks
each file once, skipping string, character, and raw string literals, and
produces the code and the comment list from that single pass, so the two can
never drift apart.

The diff mode is for bulk comment work.  When a change is supposed to touch
only comments, "code: same" on every file is the thing worth confirming.  It
compares code lines after stripping comments, stripping trailing whitespace,
and dropping blank lines, so deleting a whole line of comment does not
register as a change.  It does not collapse whitespace inside a line, because
that could hide a real change to a string literal such as "a  b" -> "a b".

    python tools/comments.py stats
    python tools/comments.py diff HEAD~1
    python tools/comments.py diff a080bfc~1 a080bfc -v

diff exits 1 when any file's code changed, so it can be used as a gate.
"""

import argparse
import difflib
import json
import os
import re
import subprocess
import sys
from collections import Counter

# Extensions whose comment syntax the C-like scanner understands.  Resource
# files and shaders use the same // and /* */ forms.
CLIKE = ('.c', '.cc', '.cpp', '.cxx', '.h', '.hpp', '.hxx', '.inl',
         '.rc', '.vert', '.frag', '.glsl', '.vs', '.fs')

WORKTREE = '--'


# --------------------------------------------------------------------------
# git access
# --------------------------------------------------------------------------

def git(*args):
    """Run git and return raw bytes, so nothing is re-encoded on the way in."""
    result = subprocess.run(['git'] + list(args), capture_output=True)
    if result.returncode != 0:
        sys.stderr.write(result.stderr.decode('utf-8', 'replace'))
        raise SystemExit('git %s failed' % ' '.join(args))
    return result.stdout


def list_sources(rev):
    """Tracked C-like files in a revision, sorted."""
    if rev == WORKTREE:
        names = git('ls-files', '-z').decode('utf-8', 'surrogateescape')
    else:
        names = git('ls-tree', '-r', '--name-only', '-z', rev).decode(
            'utf-8', 'surrogateescape')
    return sorted(p for p in names.split('\0') if p.endswith(CLIKE))


def decode_source(data):
    """Bytes to text.

    Most files are UTF-8.  A handful of the older ones carry CP1252 bytes mixed
    with it — em-dashes and quote marks from an editor that did not know better
    — so a strict UTF-8 decode fails on them.  CP1252 accepts every byte value,
    so the fallback always works and renders those characters the way the file
    meant them.  Doing this instead of decoding with errors='replace' keeps the
    text stable enough to compare between revisions.
    """
    try:
        return data.decode('utf-8')
    except UnicodeDecodeError:
        return data.decode('cp1252')


def read_source(rev, path):
    """File contents, or None when the file is absent from that revision."""
    if rev == WORKTREE:
        try:
            with open(path, 'rb') as handle:
                return decode_source(handle.read())
        except OSError:
            return None
    result = subprocess.run(['git', 'show', '%s:%s' % (rev, path)],
                            capture_output=True)
    if result.returncode != 0:
        return None
    return decode_source(result.stdout)


# --------------------------------------------------------------------------
# the lexer
# --------------------------------------------------------------------------

def skip_literal(text, start, quote):
    """Index just past a string or character literal, plus newlines crossed."""
    i, count = start + 1, 0
    while i < len(text):
        c = text[i]
        if c == '\\':
            if i + 1 < len(text) and text[i + 1] == '\n':
                count += 1
            i += 2
            continue
        if c == quote:
            return i + 1, count
        if c == '\n':
            count += 1
        i += 1
    return len(text), count


def split_comments(text):
    """Walk C-like source once, returning (code_without_comments, comments).

    Comments are dicts with the 1-based line they start on, their form, and
    their raw text including the markers.
    """
    code, comments = [], []
    i, line = 0, 1
    while i < len(text):
        ch = text[i]

        if ch == '\n':
            code.append(ch)
            line += 1
            i += 1
            continue

        # Raw string: R"delim( ... )delim"
        if ch == 'R' and i + 1 < len(text) and text[i + 1] == '"':
            open_paren = text.find('(', i + 2)
            if open_paren != -1:
                delim = text[i + 2:open_paren]
                if '\n' not in delim and len(delim) <= 16:
                    end = text.find(')' + delim + '"', open_paren)
                    end = len(text) if end == -1 else end + len(delim) + 2
                    segment = text[i:end]
                    code.append(segment)
                    line += segment.count('\n')
                    i = end
                    continue

        if ch in '"\'':
            end, crossed = skip_literal(text, i, ch)
            code.append(text[i:end])
            line += crossed
            i = end
            continue

        if ch == '/' and i + 1 < len(text) and text[i + 1] == '/':
            end = text.find('\n', i)
            end = len(text) if end == -1 else end
            # A trailing backslash continues the comment onto the next line.
            while end < len(text) and text[end - 1] == '\\':
                nxt = text.find('\n', end + 1)
                end = len(text) if nxt == -1 else nxt
            comments.append({'line': line, 'form': 'line',
                             'body': text[i:end], 'start': i})
            i = end
            continue

        if ch == '/' and i + 1 < len(text) and text[i + 1] == '*':
            end = text.find('*/', i + 2)
            end = len(text) if end == -1 else end + 2
            body = text[i:end]
            comments.append({'line': line, 'form': 'block',
                             'body': body, 'start': i})
            line += body.count('\n')
            i = end
            continue

        code.append(ch)
        i += 1

    return ''.join(code), comments


def code_lines(text):
    """Code lines, normalized enough that comment edits do not show up."""
    body, _ = split_comments(text)
    return [line.strip()
            for line in body.replace('\r\n', '\n').replace('\r', '\n').split('\n')
            if line.strip()]


def comment_lines(text):
    """Comment bodies in file order, whitespace collapsed, for comparison."""
    _, comments = split_comments(text)
    return [' '.join(c['body'].split()) for c in comments]


# --------------------------------------------------------------------------
# classification
# --------------------------------------------------------------------------

SEPARATOR = re.compile(r'^[/*=#\-_.\s\\]{6,}$')
TAG = re.compile(r'\b(TODO|FIXME|HACK|XXX|BUG|NOTE|WARNING|DEPRECATED|REVIEW|'
                 r'OPTIMIZE|SAFETY|CRITICAL|WORKAROUND|PERF)\b[:\s]', re.I)


def classify(comment):
    """One of: empty, separator, doc, tagged, normal."""
    body, form = comment['body'], comment['form']
    if form == 'line':
        content = body[2:]
    elif body.startswith('/*') and body.endswith('*/'):
        content = body[2:-2].lstrip('*')
    else:
        content = body

    stripped = content.strip()
    if not stripped:
        return 'empty'
    if SEPARATOR.match(stripped):
        return 'separator'
    head = body.lstrip()
    if head.startswith('///') or head.startswith('//!') or \
       head.startswith('/**') or head.startswith('/*!'):
        return 'doc'
    if TAG.search(content):
        return 'tagged'
    return 'normal'


def tag_of(comment):
    found = TAG.search(comment['body'])
    return found.group(1).upper() if found else None


def is_trailing(comment, text):
    """True when code sits on the same line before the comment marker."""
    line_start = text.rfind('\n', 0, comment['start']) + 1
    return bool(text[line_start:comment['start']].strip())


# --------------------------------------------------------------------------
# commands
# --------------------------------------------------------------------------

def collect(rev):
    """Every comment in a revision, tagged with file, class, and form."""
    records = []
    files = list_sources(rev)
    for path in files:
        text = read_source(rev, path)
        if text is None:
            continue
        total = text.count('\n') + 1
        _, comments = split_comments(text)
        for comment in comments:
            comment['file'] = path
            comment['kind'] = classify(comment)
            comment['tag'] = tag_of(comment)
            comment['trailing'] = is_trailing(comment, text)
            comment['file_lines'] = total
            records.append(comment)
    return records, files


def cmd_stats(args):
    records, files = collect(args.rev)
    lines = sum(c['body'].count('\n') + 1 for c in records)
    source_lines = 0
    for path in files:
        text = read_source(args.rev, path)
        if text is not None:
            source_lines += text.count('\n') + 1

    print('revision:   %s' % ('working tree' if args.rev == WORKTREE
                              else args.rev))
    print('files:      %d C-like source files' % len(files))
    print('comments:   %d' % len(records))
    print('comment lines: %d of %d source lines (%.1f%%)'
          % (lines, source_lines, lines / max(source_lines, 1) * 100))
    print('trailing (code before the marker): %d'
          % sum(1 for c in records if c['trailing']))

    for label, key in (('kind', 'kind'), ('form', 'form'), ('tag', 'tag')):
        counts = Counter(c[key] for c in records if c[key])
        print('\nby %s:' % label)
        for name, count in counts.most_common():
            print('  %-12s %d' % (name, count))

    print('\ntop 20 files:')
    for path, count in Counter(c['file'] for c in records).most_common(20):
        print('  %5d  %s' % (count, path))
    return 0


def cmd_dump(args):
    records, _ = collect(args.rev)
    for record in records:
        record.pop('start', None)
    json.dump(records, sys.stdout, indent=1, ensure_ascii=False)
    sys.stdout.write('\n')
    return 0


def cmd_diff(args):
    rev_a = args.rev_a
    rev_b = args.rev_b if args.rev_b is not None else WORKTREE

    paths = sorted(set(list_sources(rev_a)) | set(list_sources(rev_b)))
    code_moved = []
    comment_changes = []
    added_files, removed_files = [], []

    for path in paths:
        before = read_source(rev_a, path)
        after = read_source(rev_b, path)
        if before == after:
            continue

        # A file appearing or disappearing is never a comment-only change, so
        # it counts as code moving even though there is nothing to compare.
        if before is None:
            added_files.append(path)
            code_moved.append(path)
        elif after is None:
            removed_files.append(path)
            code_moved.append(path)
        elif code_lines(before) != code_lines(after):
            code_moved.append(path)

        old = comment_lines(before) if before is not None else []
        new = comment_lines(after) if after is not None else []

        added = removed = changed = 0
        details = []
        matcher = difflib.SequenceMatcher(None, old, new, autojunk=False)
        for tag, i1, i2, j1, j2 in matcher.get_opcodes():
            if tag == 'equal':
                continue
            if tag == 'insert':
                added += j2 - j1
                details += [('+', new[j]) for j in range(j1, j2)]
            elif tag == 'delete':
                removed += i2 - i1
                details += [('-', old[i]) for i in range(i1, i2)]
            else:  # replace: pair them up, the surplus counts as add or delete
                paired = min(i2 - i1, j2 - j1)
                changed += paired
                for k in range(paired):
                    details.append(('-', old[i1 + k]))
                    details.append(('+', new[j1 + k]))
                for i in range(i1 + paired, i2):
                    removed += 1
                    details.append(('-', old[i]))
                for j in range(j1 + paired, j2):
                    added += 1
                    details.append(('+', new[j]))

        if added or removed or changed:
            comment_changes.append((path, added, removed, changed, details))

    print('revision A: %s' % ('working tree' if rev_a == WORKTREE else rev_a))
    print('revision B: %s' % ('working tree' if rev_b == WORKTREE else rev_b))
    touched = ({path for path, *_ in comment_changes} | set(code_moved)
               | set(added_files) | set(removed_files))
    print('files with any change: %d' % len(touched))

    for path in added_files:
        print('\nadded file: %s' % path)
    for path in removed_files:
        print('\ndeleted file: %s' % path)

    if code_moved:
        print('\ncode changed in %d file(s):' % len(code_moved))
        for path in code_moved:
            print('  %s' % path)

    if comment_changes:
        print('\ncomment changes:')
        for path, added, removed, changed, details in comment_changes:
            verdict = 'CODE' if path in code_moved else 'code: same'
            parts = []
            if added:
                parts.append('%d added' % added)
            if removed:
                parts.append('%d removed' % removed)
            if changed:
                parts.append('%d changed' % changed)
            print('  %-52s %-12s %s' % (path, verdict, ', '.join(parts)))
            if args.verbose:
                for sign, body in details:
                    print('      %s %s' % (sign, body[:150]))
    elif not code_moved and not added_files and not removed_files:
        print('\nno differences in the C-like sources this tool reads')
        return 0

    total_added = sum(c[1] for c in comment_changes)
    total_removed = sum(c[2] for c in comment_changes)
    total_changed = sum(c[3] for c in comment_changes)
    print('\ncomments: %d added, %d removed, %d changed'
          % (total_added, total_removed, total_changed))
    if not code_moved:
        print('code: unchanged in every file that differs')
    return 1 if code_moved else 0


def main():
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(errors='replace')

    parser = argparse.ArgumentParser(
        description='Count, dump, or diff the comments in this repository.')
    subs = parser.add_subparsers(dest='command', required=True)

    p_stats = subs.add_parser('stats', help='summary counts')
    p_stats.add_argument('rev', nargs='?', default=WORKTREE)
    p_stats.set_defaults(func=cmd_stats)

    p_dump = subs.add_parser('dump', help='the corpus as JSON')
    p_dump.add_argument('rev', nargs='?', default=WORKTREE)
    p_dump.set_defaults(func=cmd_dump)

    p_diff = subs.add_parser('diff', help='comment changes between revisions')
    p_diff.add_argument('rev_a')
    p_diff.add_argument('rev_b', nargs='?', default=None)
    p_diff.add_argument('-v', '--verbose', action='store_true',
                        help='show the comment text that changed')
    p_diff.set_defaults(func=cmd_diff)

    args = parser.parse_args()
    if not os.path.isdir('.git') and not os.path.exists('.git'):
        raise SystemExit('run this from the repository root')
    return args.func(args)


if __name__ == '__main__':
    sys.exit(main())
