"""Sticky opening-mode integration against production FatFS/ReaderFile."""
import json
import shutil
import zipfile


def run(run_dir, base, invoke, cmd):
    rows = []
    latin = 'Latin only line.\n' * 40
    arabic = 'السلام عليكم\n' * 20

    def image_for(label, leading_latin=False):
        image = run_dir / (label + '.img')
        shutil.copyfile(base, image)
        text = (latin if leading_latin else '') + arabic + latin
        txt = run_dir / (label + '.txt')
        txt.write_text(text, encoding='utf-8')
        epub = run_dir / (label + '.epub')
        with zipfile.ZipFile(run_dir / 'fixtures/stored.epub') as template, zipfile.ZipFile(epub, 'w') as output:
            for entry in template.infolist():
                data = template.read(entry)
                if entry.filename.endswith('chapter.xhtml'):
                    data = ('<html><body><p>' + text.replace('\n', '</p><p>') + '</p></body></html>').encode()
                output.writestr(entry, data)
        cmd(['mcopy', '-o', '-i', image, txt, '::legacy.txt'])
        cmd(['mcopy', '-o', '-i', image, epub, '::book.epub'])
        return image, txt, epub

    def call(image, book, action, on, label, kind='-', ordinal=0):
        result = invoke(image, book, action, int(on), run_dir / label, kind, ordinal)
        rows.append(dict(case=label, result=result))
        assert result.get('on') == on, rows[-1]
        return result

    def extract(image, book, label):
        output = run_dir / label
        cmd(['mcopy', '-o', '-i', image, '::' + book, output])
        return output

    def verify_body(image, txt, epub, label):
        actual = extract(image, 'legacy.txt', label + '-actual.txt').read_bytes()
        assert actual.startswith(txt.read_bytes())
        if len(actual) > txt.stat().st_size:
            assert len(actual) == txt.stat().st_size + 800
            assert b';G=1' in actual[-800:]
        actual_epub = extract(image, 'book.epub', label + '-actual.epub')
        with zipfile.ZipFile(epub) as original, zipfile.ZipFile(actual_epub) as actual:
            assert actual.testzip() is None
            for entry in original.infolist():
                assert original.read(entry) == actual.read(entry.filename)
        assert actual_epub.read_bytes()[-22:-18] == b'PK\x05\x06'
        assert cmd(['fsck.fat', '-n', image]).returncode == 0

    # Actual automatic persistence, fresh mounts, ordinary settings/bookmark saves,
    # and sticky ON on wholly Latin resume pages, both source formats.
    image, txt, epub = image_for('mode-fresh')
    for book in ('legacy.txt', 'book.epub'):
        first = call(image, book, 'mode', True, 'first-' + book)
        assert first['result'] == 2 and first['calls'] == 1 and first['persisted_on']
        again = call(image, book, 'mode', True, 'reopen-' + book)
        assert again['result'] == 1 and again['calls'] == 0 and again['persisted_on']
        saved = call(image, book, 'mode-latin', True, 'latin-save-' + book)
        again = call(image, book, 'mode', True, 'latin-reopen-' + book)
        assert again['calls'] == 0 and again['persisted_on']
        assert again['page_anchor'] == saved['bookmark'] > 0
    verify_body(image, txt, epub, 'mode-fresh')

    # Existing OFF cache + saved Arabic anchor switches ON with state only.
    image, txt, epub = image_for('mode-cached', leading_latin=True)
    off = call(image, 'book.epub', 'mode', False, 'cached-off')
    assert off['result'] == 2 and off['calls'] == 1 and not off['persisted_on']
    saved = call(image, 'book.epub', 'mode-bookmark-arabic', False, 'cached-arabic-anchor')
    before = extract(image, 'book.epub', 'mode-cache-before.epub')
    on = call(image, 'book.epub', 'mode', True, 'cached-on')
    assert on['result'] == 2 and on['calls'] == 1 and on['persisted_on']
    assert on['bookmark'] == saved['bookmark'] > 0
    after = extract(image, 'book.epub', 'mode-cache-after.epub')
    with zipfile.ZipFile(before) as old, zipfile.ZipFile(after) as new:
        name = 'META-INF/gbareader/cache-v5'
        assert old.getinfo(name).header_offset == new.getinfo(name).header_offset
        assert old.read(name) == new.read(name)
    verify_body(image, txt, epub, 'mode-cached')

    # Returned automatic-save error is visible, retains a shaped page and ON RAM;
    # a later genuine reopen retries, with original payload integrity intact.
    for book in ('legacy.txt', 'book.epub'):
        image, txt, epub = image_for('mode-failure-' + book)
        failed = call(image, book, 'mode', True, 'failed-' + book, 'w', 1)
        assert failed['hits'] == 1 and failed['result'] == 3 and failed['calls'] == 1
        retried = call(image, book, 'mode', True, 'retried-' + book)
        assert retried['persisted_on']
        verify_body(image, txt, epub, 'mode-retry-' + book)
    (run_dir / 'mode-results.json').write_text(json.dumps(rows, indent=2) + '\n')
    print(f'PASS: {len(rows)} production opening-mode checks; TXT/original+cached EPUB, sticky Latin resume, state-only cache, failed-write retry, preserved content and fsck')
