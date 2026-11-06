#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Helper for translating salichess. See .claude/skills/add-language/SKILL.md.

  tools/i18n.py dump              write WORK/src.txt: every source string with its
                                  index and context (what a translator reads)
  tools/i18n.py export CODE       write WORK/tr/CODE.txt from the existing
                                  translations/harbour-salichess-CODE.ts
  tools/i18n.py build CODE        write translations/harbour-salichess-CODE.ts from
                                  WORK/tr/CODE.txt, checking placeholders

WORK defaults to /tmp/salichess-i18n (override with --work). A WORK/tr/CODE.txt file
has exactly one line per source string, in src.txt order: the translation, with a
literal \\n for a newline, and for a plural entry the forms separated by " ;; " in
Qt order. The source of strings is translations/harbour-salichess.ts, which the
build keeps in step with qsTr()/tr().
"""
import argparse
import os
import re
import sys
import xml.etree.ElementTree as ET
from xml.sax.saxutils import escape

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE = os.path.join(REPO, 'translations', 'harbour-salichess.ts')

# Qt plural forms per language (lupdate's rules). Pass --forms for others.
FORMS = {'de': 2, 'es': 2, 'fr': 2, 'fi': 2, 'it': 2, 'pt_BR': 2, 'pt_PT': 2,
         'nl': 2, 'sv': 2, 'da': 2, 'nb': 2, 'el': 2, 'hu': 2, 'ca': 2,
         'pl': 3, 'ru': 3, 'uk': 3, 'cs': 3, 'sk': 3, 'hr': 3, 'sr': 3,
         'tr': 1, 'zh_CN': 1, 'zh_TW': 1, 'ja': 1, 'ko': 1, 'vi': 1, 'th': 1,
         'id': 1, 'ar': 6}


def messages():
    root = ET.parse(BASE).getroot()
    return [(c.find('name').text, m) for c in root.findall('context')
            for m in c.findall('message')]


def placeholders(s):
    return sorted(x for x in re.findall(r'%\d|%n', s) if x != '%n')


def cmd_dump(a):
    os.makedirs(a.work, exist_ok=True)
    out, last = [], None
    for i, (ctx, m) in enumerate(messages()):
        if ctx != last:
            out.append('## ' + ctx)
            last = ctx
        s = (m.find('source').text or '').replace('\n', '\\n')
        out.append('%d|%s%s' % (i, s, ' [PLURAL]' if m.get('numerus') else ''))
    path = os.path.join(a.work, 'src.txt')
    open(path, 'w', encoding='utf-8').write('\n'.join(out) + '\n')
    print(path, len(messages()), 'strings')


def cmd_export(a):
    ts = os.path.join(REPO, 'translations', 'harbour-salichess-%s.ts' % a.code)
    done = {}
    for c in ET.parse(ts).getroot().findall('context'):
        for m in c.findall('message'):
            t = m.find('translation')
            forms = [(f.text or '') for f in t.findall('numerusform')] or [t.text or '']
            done[(c.find('name').text, m.find('source').text or '')] = forms
    lines = []
    for ctx, m in messages():
        forms = done.get((ctx, m.find('source').text or ''), [''])
        lines.append(' ;; '.join(f.replace('\n', '\\n') for f in forms))
    os.makedirs(os.path.join(a.work, 'tr'), exist_ok=True)
    path = os.path.join(a.work, 'tr', a.code + '.txt')
    open(path, 'w', encoding='utf-8').write('\n'.join(lines) + '\n')
    print(path)


def cmd_build(a):
    forms = a.forms or FORMS.get(a.code)
    if not forms:
        sys.exit('Unknown plural form count for %s: pass --forms N' % a.code)
    path = os.path.join(a.work, 'tr', a.code + '.txt')
    lines = open(path, encoding='utf-8').read().split('\n')
    if lines and lines[-1] == '':
        lines.pop()
    msgs = messages()
    if len(lines) != len(msgs):
        sys.exit('%s has %d lines, expected %d' % (path, len(lines), len(msgs)))
    problems = 0
    out = ['<?xml version="1.0" encoding="utf-8"?>', '<!DOCTYPE TS>',
           '<TS version="2.1" language="%s">' % a.code]
    last = None
    for i, ((ctx, m), line) in enumerate(zip(msgs, lines)):
        if ctx != last:
            if last is not None:
                out.append('</context>')
            out += ['<context>', '    <name>%s</name>' % escape(ctx)]
            last = ctx
        src = m.find('source').text or ''
        plural = m.get('numerus') == 'yes'
        parts = [p.strip() for p in line.split(';;')] if plural else [line]
        if plural and len(parts) != forms:
            print('FORMS line %d (index %d): %d forms, need %d: %s' % (i + 1, i, len(parts), forms, src))
            problems += 1
            parts = (parts + [parts[-1]] * forms)[:forms]
        for p in parts:
            if not p:
                print('EMPTY index', i, src)
                problems += 1
            elif placeholders(p) != placeholders(src):
                print('PLACEHOLDER index', i, src, '=>', p)
                problems += 1
        text = lambda s: escape(s.replace('\\n', '\n'))
        out.append('    <message%s>' % (' numerus="yes"' if plural else ''))
        out.append('        <source>%s</source>' % escape(src))
        if plural:
            out.append('        <translation>%s</translation>' % ''.join(
                '<numerusform>%s</numerusform>' % text(p) for p in parts))
        else:
            out.append('        <translation>%s</translation>' % text(parts[0]))
        out.append('    </message>')
    out += ['</context>', '</TS>', '']
    dest = os.path.join(REPO, 'translations', 'harbour-salichess-%s.ts' % a.code)
    open(dest, 'w', encoding='utf-8').write('\n'.join(out))
    print('%s: wrote %s, %d problem(s)' % (a.code, os.path.relpath(dest, REPO), problems))
    sys.exit(1 if problems else 0)


def main():
    p = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    p.add_argument('--work', default='/tmp/salichess-i18n')
    sub = p.add_subparsers(dest='cmd', required=True)
    sub.add_parser('dump').set_defaults(f=cmd_dump)
    for name, f in (('export', cmd_export), ('build', cmd_build)):
        s = sub.add_parser(name)
        s.add_argument('code')
        s.add_argument('--forms', type=int, help='plural forms (build only)')
        s.set_defaults(f=f)
    a = p.parse_args()
    a.f(a)


main()
