#!/usr/bin/env python3
"""Replace boolean game-mode flags with GameMode enum (Phase 1.1).

Approach:
- Replace reads: `if (UNDERWATER)` → `if (IsUnderwater())`
- Replace assignments: `UNDERWATER = TRUE` → `g_GameMode = GameMode::Underwater`
- Keep old flag declarations alive so any missed references still compile.
- Iteratively remove flags once all references are migrated.

This is a best-effort bulk pass. Remaining references must be fixed manually.
"""

import re
import os

# Per-flag replacement rules
# (flag_name, read_replacement, true_assign_replacement, false_assign_replacement)
FLAGS = [
    # Simple flags that map 1:1 to GameMode enum values
    ('UNDERWATER', 'IsUnderwater()',      'g_GameMode = GameMode::Underwater',  'g_GameMode = GameMode::Swimming'),
    ('SWIM',       'g_GameMode == GameMode::Swimming', 'g_GameMode = GameMode::Swimming', 'g_GameMode = GameMode::Normal'),
    ('FLY',        'g_GameMode == GameMode::Flying',   'g_GameMode = GameMode::Flying',   'g_GameMode = GameMode::Normal'),
    ('PAUSE',      'IsPaused()',           'g_GameMode = GameMode::Paused',      'g_GameMode = GameMode::Normal'),
    ('OPTICMODE',  'g_GameMode == GameMode::OpticScope', 'g_GameMode = GameMode::OpticScope', 'g_GameMode = GameMode::Normal'),
    ('BINMODE',    'g_GameMode == GameMode::Binocular',  'g_GameMode = GameMode::Binocular',  'g_GameMode = GameMode::Normal'),
    ('EXITMODE',   'g_GameMode == GameMode::ExitCountdown', 'g_GameMode = GameMode::ExitCountdown', 'g_GameMode = GameMode::Normal'),
    ('MapMode',    'g_GameMode == GameMode::MapMode',    'g_GameMode = GameMode::MapMode',    'g_GameMode = GameMode::Normal'),
    ('CrouchMode', 'g_GameMode == GameMode::Crouching',  'g_GameMode = GameMode::Crouching',  'g_GameMode = GameMode::Normal'),
    ('TrophyMode', 'g_GameMode == GameMode::TrophyMode', 'g_GameMode = GameMode::TrophyMode', 'g_GameMode = GameMode::Normal'),
    ('DogMode',    'g_GameMode == GameMode::DogMode',    'g_GameMode = GameMode::DogMode',    'g_GameMode = GameMode::Normal'),
    ('SurvivalMode','g_GameMode == GameMode::SurvivalMode','g_GameMode = GameMode::SurvivalMode','g_GameMode = GameMode::Normal'),
    ('ScannerMode','g_GameMode == GameMode::ScannerMode','g_GameMode = GameMode::ScannerMode','g_GameMode = GameMode::Normal'),
    ('SonarMode',  'g_GameMode == GameMode::SonarMode',  'g_GameMode = GameMode::SonarMode',  'g_GameMode = GameMode::Normal'),
    ('NightVisionMode','g_GameMode == GameMode::NightVision','g_GameMode = GameMode::NightVision','g_GameMode = GameMode::Normal'),
]

# Files to process
FILES = [
    'Hunt/Hunt.cpp', 'Hunt/Game.cpp', 'Hunt/GLRenderer.cpp', 'Hunt/GLUI.cpp',
    'Hunt/Characters.cpp', 'Hunt/Resources.cpp', 'Hunt/Interface.cpp',
    'Hunt/RenderSoft.cpp', 'Hunt/Render3DFX.cpp', 'Hunt/RendererD3D.cpp',
    'Hunt/Math.cpp', 'Hunt/Audio_DLL.cpp',
]


def make_read_regex(flag):
    """Match flag used in read context (not assignment, not declaration)."""
    # Match: `if (FLAG`, `FLAG ?`, `FLAG &&`, `|| FLAG`, `!FLAG`, `(FLAG)`, `FLAG)\n`, `FLAG\n`
    # Also: `FLAG;` at end of if/while/ternary
    # Not: FLAG at start of declaration line (GLOBAL, extern, etc.)
    # Not: GLOBAL lines
    return re.compile(
        r'(?<!GLOBAL\s{2,})(?<!GLOBAL )(?<!GLOBAL\t)'  # not after GLOBAL
        r'(?<!\bextern\s)'  # not after extern
        r'\b(' + re.escape(flag) + r')\b'
        r'(?!\s*=)'  # not before =
    )


def make_assign_regex(flag):
    """Match flag = TRUE/FALSE/true/false/1/0"""
    return re.compile(
        r'\b(' + re.escape(flag) + r')\s*=\s*(TRUE|FALSE|true|false|1|0)\s*;'
    )


def make_toggle_regex(flag):
    """Match flag = !flag"""
    return re.compile(
        r'\b(' + re.escape(flag) + r')\s*=\s*!\s*' + re.escape(flag) + r'\s*;'
    )


def process_file(filepath):
    with open(filepath, 'r', encoding='cp1252', errors='replace') as f:
        content = f.read()
    
    original = content
    changes = []
    
    for flag_name, read_repl, true_repl, false_repl in FLAGS:
        # 1. Replace assignments (flag = TRUE/FALSE)
        assign_re = r'\b(' + re.escape(flag_name) + r')\s*=\s*(TRUE|FALSE|true|false|1|0)\s*;'
        def make_assign_repl(flag=flag_name, t=true_repl, f=false_repl):
            def repl(m):
                val = m.group(2)
                if val in ('TRUE', 'true', '1'):
                    return t + ';'
                else:
                    return f + ';'
            return repl
        
        new_content = re.sub(assign_re, make_assign_repl(flag_name, true_repl, false_repl), content)
        if new_content != content:
            diff_count = len(re.findall(assign_re, content))
            changes.append(f"  {flag_name} assignments: {diff_count}x")
            content = new_content
        
        # 2. Replace toggles (flag = !flag)
        toggle_re = r'\b(' + re.escape(flag_name) + r')\s*=\s*!\s*' + re.escape(flag_name) + r'\s*;'
        def make_toggle_repl(flag=flag_name, t=true_repl, f=false_repl):
            def repl(m):
                return f'g_GameMode = (g_GameMode == {t.split("=")[1].strip()}) ? GameMode::Normal : {t.split("=")[1].strip()};'
            return repl
        
        # Actually, for toggle like PAUSE = !PAUSE:
        # We want: g_GameMode = IsPaused() ? GameMode::Normal : GameMode::Paused;
        if flag_name == 'PAUSE':
            toggle_pause_re = r'\bPAUSE\s*=\s*!\s*PAUSE\s*;'
            new_content = re.sub(toggle_pause_re, 'g_GameMode = IsPaused() ? GameMode::Normal : GameMode::Paused;', content)
            if new_content != content:
                changes.append(f"  PAUSE toggle: 1x")
                content = new_content
        
        # 3. Replace reads (flag used as boolean)
        # Only replace standalone flag references, not in declarations or assignments
        # We process line by line to keep it simple
        
        lines = content.split('\n')
        new_lines = []
        for line in lines:
            stripped = line.strip()
            # Skip GLOBAL declaration lines entirely
            if stripped.startswith('GLOBAL') and flag_name in stripped:
                new_lines.append(line)
                continue
            # Skip extern declaration lines
            if stripped.startswith('extern') and flag_name in stripped:
                new_lines.append(line)
                continue
            # Skip lines that are just a declaration (already handled assignments above)
            # Now check for read patterns
            # Match `flag` when not in `flag = X` or `_flag` or `GLOBAL flag`
            # Simple: replace standalone `flag` word with read_repl
            # But be careful with things like `UNDERWATER && X` -> `IsUnderwater() && X`
            # We already handled assignments above, so remaining flag_NAME is a read
            
            # Simple word-boundary replacement, skip if already modified by assignment
            if not line.strip().endswith('//') and not line.strip().startswith('//'):
                # Replace the flag word with the read replacement
                # Be careful not to double-replace if already done by assignment
                
                # Use word boundary but exclude after GLOBAL
                pattern = r'(?<!\w)' + re.escape(flag_name) + r'(?!\w)'
                
                # Only replace if not preceded by GLOBAL (with various spacing)
                # and not in assignment context (which we already handled)
                # and not the full word is part of a longer identifier
                
                # Check: does the line have the flag word?
                if re.search(pattern, line) and flag_name in line:
                    # Don't replace in GLOBAL declarations
                    if 'GLOBAL' in line and flag_name in line.split('GLOBAL')[0] if 'GLOBAL' in line else False:
                        pass
                    elif f'{flag_name} ' in line or f'{flag_name})' in line or f'{flag_name}?' in line or f'{flag_name}&&' in line or f'&&{flag_name}' in line or f'||{flag_name}' in line or f'{flag_name}||' in line or f'!{flag_name}' in line or f'({flag_name}' in line or f'{flag_name}:' in line:
                        # This is a read context
                        line = re.sub(pattern, read_repl, line)
            
            new_lines.append(line)
        
        new_content = '\n'.join(new_lines)
        if new_content != content:
            content = new_content
    
    if content != original:
        with open(filepath, 'w', encoding='cp1252', errors='replace', newline='') as f:
            f.write(content)
        print(f"  Modified: {filepath}")
        for c in changes:
            print(c)
        return True
    return False


def main():
    total = 0
    for filepath in FILES:
        if not os.path.exists(filepath):
            print(f"  SKIP: {filepath}")
            continue
        try:
            if process_file(filepath):
                total += 1
        except Exception as e:
            print(f"  ERROR {filepath}: {e}")
            import traceback
            traceback.print_exc()
    print(f"\nTotal files modified: {total}")


if __name__ == '__main__':
    main()
