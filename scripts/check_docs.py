#!/usr/bin/env python3
"""Check documentation links, public catalog coverage and sitemap without dependencies."""
from pathlib import Path
from urllib.parse import unquote, urlsplit
import argparse
import re
import xml.etree.ElementTree as ET


def check(root: Path, site_dir: Path | None = None) -> list[str]:
    errors = []
    sources = [root / 'README.md', root / 'README.zh-CN.md', root / 'CONTRIBUTING.md']
    sources += list((root / 'docs').rglob('*.md')) + list((root / 'internal').rglob('*.md'))
    for source in sources:
        content = source.read_text(encoding='utf-8')
        content = re.sub(r'^```[^\n]*\n.*?^```\s*$', '', content, flags=re.M | re.S)
        for href in re.findall(r'!?\[[^\]\n]*\]\(([^\s)]+)\)', content):
            parts = urlsplit(href)
            if parts.scheme or parts.netloc or not parts.path:
                continue
            target = (source.parent / unquote(parts.path)).resolve()
            if not target.exists():
                errors.append(f'{source.relative_to(root)}: missing link {href}')
    site = site_dir or root / 'site'
    app = (site / 'app.js').read_text(encoding='utf-8')
    catalog = re.findall(r'href: "(?:\.\./|\./)(docs/[^"#]+\.md)"', app)
    public_docs = {p.relative_to(root).as_posix() for p in (root / 'docs').rglob('*.md')
                   if 'releases' not in p.relative_to(root / 'docs').parts}
    if len(catalog) != len(set(catalog)):
        errors.append('site/app.js: duplicate document entries')
    if set(catalog) != public_docs:
        errors.append(f'site catalog mismatch: missing={sorted(public_docs - set(catalog))}, stale={sorted(set(catalog) - public_docs)}')
    urls = [unquote(node.text or '') for node in ET.parse(site / 'sitemap.xml').iter('{http://www.sitemaps.org/schemas/sitemap/0.9}loc')]
    prefix = 'https://sudoevolve.github.io/EUI-NEO/'
    expected = {prefix, prefix + 'llms.txt'} | {prefix + path for path in catalog}
    if len(urls) != len(set(urls)) or set(urls) != expected:
        errors.append('site/sitemap.xml: entries must match the public catalog exactly, without duplicates')
    for name in ['index.html', 'app.js', 'llms.txt']:
        content = (site / name).read_text(encoding='utf-8')
        for path in re.findall(r'(?:\.\./|\./)(?:docs|assets)/[^"\s<>\)]+', content):
            target = site / path if site_dir else root / path.removeprefix('../')
            if not target.exists():
                errors.append(f'{name}: missing resource {path}')
    if site_dir and (site / 'internal').exists():
        errors.append('published site must not include internal records')
    return errors


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--site-dir', type=Path, help='also validate a prepared Pages directory')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    errors = check(root, args.site_dir.resolve() if args.site_dir else None)
    for error in errors:
        print(error)
    print(f'documentation check: {len(errors)} error(s)')
    raise SystemExit(bool(errors))
