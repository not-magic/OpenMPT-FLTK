#!/usr/bin/env python3
"""Converts mptrack.rc into C++ data tables: dialog templates, menus, string tables and embedded binary resources.

Usage (from src/): tools/rc2cpp.py mptrack/mptrack.rc mptrack/resource.h mptrack/res/ResourceData.cpp
"""
import os
import re
import sys

STYLE = {}
def defs(prefix, items):
    for k, v in items.items():
        STYLE[prefix + k] = v

defs('WS_', dict(POPUP=0x80000000, CHILD=0x40000000, VISIBLE=0x10000000, DISABLED=0x08000000, CLIPSIBLINGS=0x04000000, CLIPCHILDREN=0x02000000,
    MAXIMIZE=0x01000000, BORDER=0x00800000, DLGFRAME=0x00400000, CAPTION=0x00C00000, VSCROLL=0x00200000, HSCROLL=0x00100000, SYSMENU=0x00080000,
    THICKFRAME=0x00040000, GROUP=0x00020000, TABSTOP=0x00010000, MINIMIZEBOX=0x00020000, MAXIMIZEBOX=0x00010000, OVERLAPPED=0, MINIMIZE=0x20000000))
defs('WS_EX_', dict(TOOLWINDOW=0x80, CLIENTEDGE=0x200, STATICEDGE=0x20000, TRANSPARENT=0x20, ACCEPTFILES=0x10, CONTROLPARENT=0x10000, CONTEXTHELP=0x400, MODALFRAME=1,
    NOPARENTNOTIFY=4, LEFT=0, RIGHT=0x1000, DLGMODALFRAME=1))
STYLE['BS_3STATE'] = 5
STYLE['DS_3DLOOK'] = 4
STYLE['WS_EX_APPWINDOW'] = 0x40000
STYLE['WS_EX_NOACTIVATE'] = 0x8000000
defs('DS_', dict(SETFONT=0x40, MODALFRAME=0x80, FIXEDSYS=0x0008, CENTER=0x0800, SETFOREGROUND=0x200, _3DLOOK=0x4, CONTROL=0x400, ABSALIGN=1, NOFAILCREATE=0x10, LOCALEDIT=0x20,
    SYSMODAL=2, NOIDLEMSG=0x100, CONTEXTHELP=0x2000, CENTERMOUSE=0x1000))
defs('BS_', dict(PUSHBUTTON=0, DEFPUSHBUTTON=1, CHECKBOX=2, AUTOCHECKBOX=3, RADIOBUTTON=4, _3STATE=5, AUTO3STATE=6, GROUPBOX=7, AUTORADIOBUTTON=9, OWNERDRAW=0xB,
    LEFTTEXT=0x20, PUSHLIKE=0x1000, FLAT=0x8000, MULTILINE=0x2000, SPLITBUTTON=0xC, ICON=0x40, BITMAP=0x80, LEFT=0x100, RIGHT=0x200, CENTER=0x300, TOP=0x400, BOTTOM=0x800,
    VCENTER=0xC00, NOTIFY=0x4000, TEXT=0, COMMANDLINK=0xE, DEFCOMMANDLINK=0xF, DEFSPLITBUTTON=0xD))
defs('ES_', dict(LEFT=0, CENTER=1, RIGHT=2, MULTILINE=4, UPPERCASE=8, LOWERCASE=0x10, PASSWORD=0x20, AUTOVSCROLL=0x40, AUTOHSCROLL=0x80, NOHIDESEL=0x100,
    OEMCONVERT=0x400, READONLY=0x800, WANTRETURN=0x1000, NUMBER=0x2000))
defs('SS_', dict(LEFT=0, CENTER=1, RIGHT=2, ICON=3, BLACKRECT=4, GRAYRECT=5, WHITERECT=6, BLACKFRAME=7, GRAYFRAME=8, WHITEFRAME=9, USERITEM=0xA, SIMPLE=0xB,
    LEFTNOWORDWRAP=0xC, OWNERDRAW=0xD, BITMAP=0xE, ENHMETAFILE=0xF, ETCHEDHORZ=0x10, ETCHEDVERT=0x11, ETCHEDFRAME=0x12, NOPREFIX=0x80, NOTIFY=0x100, CENTERIMAGE=0x200,
    RIGHTJUST=0x400, REALSIZEIMAGE=0x800, SUNKEN=0x1000, EDITCONTROL=0x2000, ENDELLIPSIS=0x4000, PATHELLIPSIS=0x8000, WORDELLIPSIS=0xC000, REALSIZECONTROL=0x40))
defs('CBS_', dict(SIMPLE=1, DROPDOWN=2, DROPDOWNLIST=3, OWNERDRAWFIXED=0x10, OWNERDRAWVARIABLE=0x20, AUTOHSCROLL=0x40, OEMCONVERT=0x80, SORT=0x100, HASSTRINGS=0x200,
    NOINTEGRALHEIGHT=0x400, DISABLENOSCROLL=0x800, UPPERCASE=0x2000, LOWERCASE=0x4000))
defs('LBS_', dict(NOTIFY=1, SORT=2, NOREDRAW=4, MULTIPLESEL=8, OWNERDRAWFIXED=0x10, OWNERDRAWVARIABLE=0x20, HASSTRINGS=0x40, USETABSTOPS=0x80, NOINTEGRALHEIGHT=0x100,
    MULTICOLUMN=0x200, WANTKEYBOARDINPUT=0x400, EXTENDEDSEL=0x800, DISABLENOSCROLL=0x1000, NODATA=0x2000, NOSEL=0x4000, STANDARD=0xA00003))
defs('LVS_', dict(ICON=0, REPORT=1, SMALLICON=2, LIST=3, SINGLESEL=4, SHOWSELALWAYS=8, SORTASCENDING=0x10, SORTDESCENDING=0x20, SHAREIMAGELISTS=0x40, NOLABELWRAP=0x80,
    AUTOARRANGE=0x100, EDITLABELS=0x200, OWNERDATA=0x1000, NOSCROLL=0x2000, ALIGNLEFT=0x800, ALIGNTOP=0, NOCOLUMNHEADER=0x4000, NOSORTHEADER=0x8000, OWNERDRAWFIXED=0x400))
defs('LVS_EX_', dict(GRIDLINES=1, FULLROWSELECT=0x20, CHECKBOXES=4, HEADERDRAGDROP=0x10))
defs('TBS_', dict(AUTOTICKS=1, VERT=2, HORZ=0, TOP=4, BOTTOM=0, LEFT=4, RIGHT=0, BOTH=8, NOTICKS=0x10, ENABLESELRANGE=0x20, FIXEDLENGTH=0x40, NOTHUMB=0x80, TOOLTIPS=0x100))
defs('UDS_', dict(WRAP=1, SETBUDDYINT=2, ALIGNRIGHT=4, ALIGNLEFT=8, AUTOBUDDY=0x10, ARROWKEYS=0x20, HORZ=0x40, NOTHOUSANDS=0x80, HOTTRACK=0x100))
defs('TVS_', dict(HASBUTTONS=1, HASLINES=2, LINESATROOT=4, EDITLABELS=8, DISABLEDRAGDROP=0x10, SHOWSELALWAYS=0x20, RTLREADING=0x40, NOTOOLTIPS=0x80, CHECKBOXES=0x100,
    TRACKSELECT=0x200, SINGLEEXPAND=0x400, INFOTIP=0x800, FULLROWSELECT=0x1000, NOSCROLL=0x2000, NONEVENHEIGHT=0x4000, NOHSCROLL=0x8000))
defs('TCS_', dict(TABS=0, BUTTONS=0x100, SINGLELINE=0, MULTILINE=0x200, RIGHTJUSTIFY=0, FIXEDWIDTH=0x400, FOCUSONBUTTONDOWN=0x1000, TOOLTIPS=0x4000, FLATBUTTONS=8,
    FORCEICONLEFT=0x10, FORCELABELLEFT=0x20, HOTTRACK=0x40, VERTICAL=0x80, BOTTOM=2, RIGHT=2))
defs('PBS_', dict(SMOOTH=1, VERTICAL=4, MARQUEE=8))
defs('SBS_', dict(HORZ=0, VERT=1, TOPALIGN=2, BOTTOMALIGN=4, SIZEBOX=8))
defs('LWS_', dict(TRANSPARENT=1, IGNORERETURN=2, NOPREFIX=4, USEVISUALSTYLE=8, USECUSTOMTEXT=0x10, RIGHT=0x20))
defs('CCS_', dict(TOP=1, NOMOVEY=2, BOTTOM=3, NORESIZE=4, NOPARENTALIGN=8, ADJUSTABLE=0x20, NODIVIDER=0x40, VERT=0x80, LEFT=0x81, NOMOVEX=0x82, RIGHT=0x83))
defs('TBSTYLE_', dict(BUTTON=0, SEP=1, CHECK=2, GROUP=4, CHECKGROUP=6, DROPDOWN=8, AUTOSIZE=0x10, NOPREFIX=0x20, TOOLTIPS=0x100, WRAPABLE=0x200, ALTDRAG=0x400,
    FLAT=0x800, LIST=0x1000, CUSTOMERASE=0x2000, REGISTERDROP=0x4000, TRANSPARENT=0x8000))
STYLE['NOT'] = None

def eval_style(expr, warn):
    expr = expr.strip()
    if not expr:
        return 0, 0
    value = 0
    clear = 0
    negate = False
    for tok in re.split(r'\s*\|\s*', expr):
        tok = tok.strip()
        parts = tok.split()
        neg = False
        if parts and parts[0] == 'NOT':
            neg = True
            tok = ' '.join(parts[1:])
        if re.fullmatch(r'0[xX][0-9a-fA-F]+', tok):
            v = int(tok, 16)
        elif re.fullmatch(r'\d+', tok):
            v = int(tok)
        elif tok in STYLE and STYLE[tok] is not None:
            v = STYLE[tok]
        else:
            warn('unknown style %r' % tok)
            v = 0
        if neg:
            clear |= v
        else:
            value |= v
    return value & 0xFFFFFFFF, clear & 0xFFFFFFFF

def tokenize_args(s):
    """Split a comma-separated rc argument list honoring quoted strings ("" escapes)."""
    args = []
    cur = ''
    i = 0
    inq = False
    while i < len(s):
        c = s[i]
        if inq:
            if c == '"':
                if i + 1 < len(s) and s[i + 1] == '"':
                    cur += '""'
                    i += 2
                    continue
                inq = False
            cur += c
        else:
            if c == '"':
                inq = True
                cur += c
            elif c == ',':
                args.append(cur.strip())
                cur = ''
            else:
                cur += c
        i += 1
    if cur.strip() != '' or args:
        args.append(cur.strip())
    return args

def unquote(s):
    s = s.strip()
    if len(s) >= 2 and s[0] == '"' and s[-1] == '"':
        s = s[1:-1].replace('""', '"')
    return s

def cstr(s):
    """C++ string literal (UTF-8 passthrough, escapes for rc backslash sequences)."""
    out = ''
    i = 0
    while i < len(s):
        c = s[i]
        if c == '\\' and i + 1 < len(s):
            n = s[i + 1]
            if n == 'n': out += '\\n'
            elif n == 't': out += '\\t'
            elif n == 'r': out += '\\r'
            elif n == '\\': out += '\\\\'
            elif n == '"': out += '\\"'
            elif n in '01234567':
                j = i + 1
                k = j
                while k < len(s) and k < j + 3 and s[k] in '01234567': k += 1
                out += '\\%03o' % int(s[j:k], 8)
                i = k
                continue
            else:
                out += '\\\\' + n
            i += 2
            continue
        if c == '"': out += '\\"'
        elif c == '\n': out += '\\n'
        elif ord(c) < 32: out += '\\%03o' % ord(c)
        elif c == '?': out += '\\?'
        else: out += c
        i += 1
    return '"' + out + '"'

def join_continuations(lines):
    out = []
    buf = None
    for ln in lines:
        stripped = ln.rstrip('\n').rstrip()
        if buf is not None:
            buf += ' ' + stripped.strip()
            if not (stripped.endswith('|') or stripped.endswith(',')):
                out.append(buf); buf = None
            continue
        if stripped.endswith('|') or (stripped.endswith(',') and not stripped.lstrip().startswith('//')):
            buf = stripped
            continue
        out.append(stripped)
    if buf is not None: out.append(buf)
    return out

def main():
    rc_path, h_path, out_path = sys.argv[1:4]
    res_dir = os.path.dirname(rc_path)
    warnings = []
    def warn(m): warnings.append(m)

    # resource.h ids
    ids = {}
    for ln in open(h_path, encoding='utf-8', errors='replace'):
        m = re.match(r'#define\s+(\w+)\s+(-?\d+|0x[0-9a-fA-F]+)', ln)
        if m: ids[m.group(1)] = int(m.group(2), 0)
    # ids defined by afxres.h that the rc uses
    ids.setdefault('IDOK', 1); ids.setdefault('IDCANCEL', 2); ids.setdefault('IDC_STATIC', -1)

    text = open(rc_path, encoding='utf-8', errors='replace').read()
    # Drop non-English / designer-only sections
    lines = join_continuations(text.split('\n'))
    # Merge a line consisting only of an ID with the following quoted string in STRINGTABLE
    dialogs = []
    menus = []
    strings = []
    files = []
    unknown_ids = set()
    layouts = {}

    def idexpr(tok):
        tok = tok.strip()
        if re.fullmatch(r'-?\d+', tok): return tok
        if re.fullmatch(r'0[xX][0-9a-fA-F]+', tok): return tok
        if tok == 'IDC_STATIC': return '-1'
        if tok not in ids: unknown_ids.add(tok)
        return tok

    i = 0
    n = len(lines)
    while i < n:
        ln = lines[i].strip()
        # file resources
        m = re.match(r'^(\w+)\s+(ICON|BITMAP|CURSOR|PNG|RT_RCDATA)\s+"([^"]+)"', ln)
        if m:
            rid, rtype, path = m.groups()
            path = path.replace('\\\\', '/').replace('\\', '/')
            files.append((rid, rtype, path)); i += 1; continue
        m = re.match(r'^(\w+)\s+DIALOGEX?\s+(.*)$', ln)
        if m:
            did = m.group(1)
            args = [a.strip() for a in m.group(2).split(',')]
            x, y, w, h = [int(a) for a in args[:4]]
            d = dict(id=did, x=x, y=y, w=w, h=h, caption='', style=0, exstyle=0, font=None, controls=[])
            i += 1
            while i < n and lines[i].strip() != 'BEGIN':
                hl = lines[i].strip()
                if hl.startswith('STYLE'):
                    d['style'], _ = eval_style(hl[5:], warn)
                elif hl.startswith('EXSTYLE'):
                    d['exstyle'], _ = eval_style(hl[7:], warn)
                elif hl.startswith('CAPTION'):
                    d['caption'] = unquote(hl[7:].strip())
                elif hl.startswith('FONT'):
                    fa = tokenize_args(hl[4:])
                    d['font'] = (int(fa[0]), unquote(fa[1]) if len(fa) > 1 else '')
                i += 1
            i += 1
            while i < n and lines[i].strip() != 'END':
                cl = lines[i].strip(); i += 1
                if not cl or cl.startswith('//'): continue
                mm = re.match(r'^(\w+)\s+(.*)$', cl)
                if not mm: continue
                kw, rest = mm.groups()
                a = tokenize_args(rest)
                c = dict(kind=kw, text='', id='0', cls='', style=0, exstyle=0, x=0, y=0, w=0, h=0, clear=0)
                try:
                    if kw == 'CONTROL':
                        c['text'] = unquote(a[0]); c['id'] = idexpr(a[1]); c['cls'] = unquote(a[2])
                        c['style'], c['clear'] = eval_style(a[3], warn)
                        c['x'], c['y'], c['w'], c['h'] = [int(v) for v in a[4:8]]
                        if len(a) > 8: c['exstyle'], _ = eval_style(a[8], warn)
                    elif kw in ('EDITTEXT', 'COMBOBOX', 'LISTBOX', 'SCROLLBAR'):
                        c['id'] = idexpr(a[0])
                        c['x'], c['y'], c['w'], c['h'] = [int(v) for v in a[1:5]]
                        if len(a) > 5: c['style'], c['clear'] = eval_style(a[5], warn)
                        if len(a) > 6: c['exstyle'], _ = eval_style(a[6], warn)
                    else:
                        c['text'] = unquote(a[0]); c['id'] = idexpr(a[1])
                        c['x'], c['y'], c['w'], c['h'] = [int(v) for v in a[2:6]]
                        if len(a) > 6: c['style'], c['clear'] = eval_style(a[6], warn)
                        if len(a) > 7: c['exstyle'], _ = eval_style(a[7], warn)
                except Exception as e:
                    warn('dialog %s: cannot parse %r (%s)' % (did, cl, e))
                    continue
                d['controls'].append(c)
            dialogs.append(d)
            continue
        m = re.match(r'^(\w+)\s+AFX_DIALOG_LAYOUT\s*$', ln)
        if m:
            lid = m.group(1)
            i += 1
            while lines[i].strip() != 'BEGIN': i += 1
            i += 1
            nums = []
            while lines[i].strip() != 'END':
                nums += [int(v) for v in re.findall(r'-?\d+', lines[i])]
                i += 1
            layouts[lid] = nums[1:]  # first value is the version
            continue
        m = re.match(r'^(\w+)\s+MENU\s*$', ln)
        if m:
            mid = m.group(1)
            i += 1
            while lines[i].strip() != 'BEGIN': i += 1
            i += 1
            def parse_items():
                nonlocal i
                items = []
                while i < n:
                    il = lines[i].strip(); i += 1
                    if il == 'END': return items
                    if il.startswith('POPUP'):
                        label = unquote(tokenize_args(il[5:])[0])
                        while lines[i].strip() != 'BEGIN': i += 1
                        i += 1
                        items.append(dict(label=label, popup=parse_items()))
                    elif il.startswith('MENUITEM'):
                        r = il[8:].strip()
                        if r.startswith('SEPARATOR'):
                            items.append(dict(label=None, sep=True))
                        else:
                            a = tokenize_args(r)
                            flags = ' '.join(a[2:]) if len(a) > 2 else ''
                            items.append(dict(label=unquote(a[0]), id=idexpr(a[1]) if len(a) > 1 else '0', flags=flags))
                return items
            menus.append((mid, parse_items()))
            continue
        if ln.startswith('STRINGTABLE'):
            i += 1
            while lines[i].strip() != 'BEGIN': i += 1
            i += 1
            pending = None
            while i < n and lines[i].strip() != 'END':
                sl = lines[i].strip(); i += 1
                if not sl or sl.startswith('//'): continue
                if pending is not None:
                    strings.append((pending, unquote(sl))); pending = None; continue
                mm = re.match(r'^(\w+)\s*(".*")?\s*$', sl)
                if mm and mm.group(2) is None:
                    pending = mm.group(1); continue
                mm = re.match(r'^(\w+)\s+(".*")\s*$', sl)
                if mm:
                    strings.append((mm.group(1), unquote(mm.group(2))))
            continue
        i += 1

    o = []
    o.append('// Generated by src/tools/rc2cpp.py from src/mptrack/mptrack.rc. Do not edit.\n')
    o.append('#include "stdafx.h"\n#include "../ui/ResourceTables.h"\n#include "../resource.h"\n#include "../ui/StandardIds.h"\n\nOPENMPT_NAMESPACE_BEGIN\n\nnamespace ui\n{\n')

    # binary files
    for rid, rtype, path in files:
        if rtype in ('ICON', 'CURSOR'):
            continue
        full = os.path.join(res_dir, path)
        if not os.path.exists(full):
            # case-insensitive lookup
            d = os.path.dirname(full)
            cand = [f for f in os.listdir(d) if f.lower() == os.path.basename(full).lower()] if os.path.isdir(d) else []
            if not cand:
                warn('missing file %s' % path); continue
            full = os.path.join(d, cand[0])
        data = open(full, 'rb').read()
        o.append('static const uint8 data_%s[] = {' % rid)
        for k in range(0, len(data), 24):
            o.append(','.join(str(b) for b in data[k:k+24]) + ',')
        o.append('};\n')
    o.append('const BinaryResource binaryResources[] = {')
    for rid, rtype, path in files:
        if rtype in ('ICON', 'CURSOR'): continue
        if not os.path.exists(os.path.join(res_dir, path)) and not any(True for _ in []):
            pass
        o.append('\t{%s, ResourceType::%s, data_%s, sizeof(data_%s)},' % (rid, 'Png' if rtype == 'PNG' else ('Bitmap' if rtype == 'BITMAP' else 'Data'), rid, rid))
    o.append('};\nconst std::size_t binaryResourceCount = std::size(binaryResources);\n')

    # strings
    o.append('const StringResource stringResources[] = {')
    for sid, s in strings:
        if sid.startswith('AFX_IDS_'): continue
        o.append('\t{%s, %s},' % (idexpr(sid), cstr(s)))
    o.append('};\nconst std::size_t stringResourceCount = std::size(stringResources);\n')

    # dialogs
    ctl = 0
    for d in dialogs:
        if not d['controls']:
            continue
        o.append('static const DialogControl controls_%s[] = {' % d['id'])
        for c in d['controls']:
            o.append('\t{ControlKind::%s, %s, %s, "%s", %d, %d, %d, %d, 0x%X, 0x%X, 0x%X},' % (
                c['kind'].capitalize() if c['kind'] != 'DEFPUSHBUTTON' else 'DefPushButton', c['id'], cstr(c['text']), c['cls'], c['x'], c['y'], c['w'], c['h'], c['style'], c['clear'], c['exstyle']))
        o.append('};')
    for did, nums in layouts.items():
        if nums:
            o.append('static const int16 layout_%s[] = {%s};' % (did, ','.join(str(v) for v in nums)))
    o.append('const DialogTemplate dialogTemplates[] = {')
    for d in dialogs:
        fnt = d['font'] or (8, '')
        if d['controls']:
            lay = ('layout_%s, std::size(layout_%s)' % (d['id'], d['id'])) if layouts.get(d['id']) else 'nullptr, 0'
            o.append('\t{%s, %d, %d, %d, %d, %s, 0x%X, 0x%X, %d, controls_%s, std::size(controls_%s), %s},' % (d['id'], d['x'], d['y'], d['w'], d['h'], cstr(d['caption']), d['style'], d['exstyle'], fnt[0], d['id'], d['id'], lay))
        else:
            o.append('\t{%s, %d, %d, %d, %d, %s, 0x%X, 0x%X, %d, nullptr, 0, nullptr, 0},' % (d['id'], d['x'], d['y'], d['w'], d['h'], cstr(d['caption']), d['style'], d['exstyle'], fnt[0]))
    o.append('};\nconst std::size_t dialogTemplateCount = std::size(dialogTemplates);\n')

    # menus
    mcount = [0]
    def emit_menu(mid, items):
        name = 'menu_%s' % mid
        def emit(items, nm):
            for it in items:
                if 'popup' in it:
                    sub = '%s_%d' % (nm, mcount[0]); mcount[0] += 1
                    emit(it['popup'], sub)
                    it['sub'] = sub
            o.append('static const MenuEntry %s[] = {' % nm)
            for it in items:
                if it.get('sep'):
                    o.append('\t{nullptr, 0, 0, nullptr, 0},')
                elif 'popup' in it:
                    o.append('\t{%s, 0, 0, %s, std::size(%s)},' % (cstr(it['label']), it['sub'], it['sub']))
                else:
                    fl = 0
                    f = it['flags']
                    if 'GRAYED' in f: fl |= 1
                    if 'CHECKED' in f: fl |= 2
                    if 'INACTIVE' in f: fl |= 4
                    o.append('\t{%s, %s, %d, nullptr, 0},' % (cstr(it['label']), it['id'], fl))
            o.append('};')
        emit(items, name)
    for mid, items in menus:
        emit_menu(mid, items)
    o.append('const MenuTemplate menuTemplates[] = {')
    for mid, items in menus:
        o.append('\t{%s, menu_%s, std::size(menu_%s)},' % (mid, mid, mid))
    o.append('};\nconst std::size_t menuTemplateCount = std::size(menuTemplates);\n')
    o.append('}  // namespace ui\n\nOPENMPT_NAMESPACE_END\n')
    open(out_path, 'w', encoding='utf-8').write('\n'.join(o))
    sys.stderr.write('dialogs=%d menus=%d strings=%d files=%d\n' % (len(dialogs), len(menus), len(strings), len(files)))
    if unknown_ids:
        sys.stderr.write('unknown ids (%d): %s\n' % (len(unknown_ids), ' '.join(sorted(unknown_ids))))
    for w in sorted(set(warnings)):
        sys.stderr.write('warning: ' + w + '\n')

main()
