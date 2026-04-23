import glob, re

files = glob.glob(r'C:\firestorm\phoenix-firestorm\indra\**\*.cpp', recursive=True)
files += glob.glob(r'C:\firestorm\phoenix-firestorm\indra\**\*.h', recursive=True)

count = 0
for f in files:
    d = open(f, 'rb').read()
    if any(b > 127 for b in d):
        # UTF-8の特殊文字をASCIIに置換
        replacements = [
            (b'\xe2\x80\x93', b'-'),
            (b'\xe2\x80\x94', b'-'),
            (b'\xe2\x80\x98', b"'"),
            (b'\xe2\x80\x99', b"'"),
            (b'\xe2\x80\x9c', b'"'),
            (b'\xe2\x80\x9d', b'"'),
            (b'\xe2\x89\xa4', b'<='),
            (b'\xe2\x89\xa5', b'>='),
            (b'\xc3\x97', b'x'),
            (b'\xc2\xa0', b' '),
            (b'\xe2\x80\xa6', b'...'),
        ]
        for old, new in replacements:
            d = d.replace(old, new)
        # 残った非ASCIIバイトを除去
        d = bytes(b for b in d if b < 128)
        open(f, 'wb').write(d)
        count += 1

print('fixed:', count, 'files')