#!/usr/bin/env python3
"""Replace wsprintf/wsprintfA with sprintf_s using known buffer sizes."""

import re
import os

# Known buffer sizes per file
BUF_SIZES = {
    'Hunt/Audio_DLL.cpp':  {'buf': 128, 'm': 128, 'logt': 128},
    'Hunt/Characters.cpp': {'t': 32, 'logt': 128},
    'Hunt/Game.cpp':       {'logt': 128, 'fname': 128, 'fname2': 128, 'msg': 128},
    'Hunt/GLRenderer.cpp': {'t': 64},
    'Hunt/GLUI.cpp':       {'buf': 128, 't': 32},
    'Hunt/Hunt.cpp':       {'buf': 200, 'logt': 128},
    'Hunt/Interface.cpp':  {'logt': 128},
    'Hunt/Memory.h':       {'buf': 256},
    'Hunt/Render3DFX.cpp': {'t': 128, 'logt': 128, 'buf': 128},
    'Hunt/RenderSoft.cpp': {'t': 32, 'logt': 128, 'buf': 128},
    'Hunt/RendererD3D.cpp':{'t': 128, 'logt': 128, 'buf': 128},
    'Hunt/Resources.cpp':  {'sz': 512, 't': 12, 'logt': 128, 'MapName': 128, 'RscName': 128},
}

def process_file(filepath):
    sizes = BUF_SIZES[filepath]
    with open(filepath, 'r', encoding='cp1252', errors='replace') as f:
        content = f.read()

    changes = 0
    new_content = content

    # Process each known buffer variable
    for varname, bufsz in sorted(sizes.items(), key=lambda x: -len(x[0])):
        # Build pattern for wsprintf(varname, ...) or wsprintfA(varname, ...)
        # We need to match these WITHOUT capturing the rest of the arguments
        
        # Pattern: wsprintf(varname, or wsprintfA(varname,
        # We need to be careful not to match inside comments (but replacing inside
        # comments is fine - it's dead code)
        
        old1 = f'wsprintf({varname},'
        new1 = f'sprintf_s({varname}, sizeof({varname}),'
        
        old2 = f'wsprintfA({varname},'
        new2 = f'sprintf_s({varname}, sizeof({varname}),'
        
        count = new_content.count(old1) + new_content.count(old2)
        if count > 0:
            new_content = new_content.replace(old1, new1)
            new_content = new_content.replace(old2, new2)
            changes += count
            print(f"  {varname}[{bufsz}]: {count}x")

    with open(filepath, 'w', encoding='cp1252', errors='replace', newline='') as f:
        f.write(new_content)
    return changes


def main():
    total = 0
    for filepath in sorted(BUF_SIZES.keys()):
        if not os.path.exists(filepath):
            print(f"SKIP: {filepath}")
            continue
        print(f"\n{filepath}:")
        try:
            n = process_file(filepath)
            total += n
            print(f"  -> {n} replacements")
        except Exception as e:
            print(f"  ERROR: {e}")
            import traceback
            traceback.print_exc()
    print(f"\nTotal: {total} replacements")


if __name__ == '__main__':
    main()
