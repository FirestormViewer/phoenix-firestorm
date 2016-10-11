The NotoSansCombined.ttf file included in this directory is the two font
files NotoSans-Regular.ttf and NotoSansSymbols-Regular.ttf merged into one.
No other changes were made. This was necessary because the two Unicode
characters U+2714, HEAVY CHECK MARK, and U+25B6, BLACK RIGHT-POINTING
TRIANGLE, are in the Symbol font file, not the base file where the viewer
(specifically, the menu building code in llui/llmenugl.cpp) expects to find
it. This also allows proper display of many other symbols, as well.

The viewer really should allow specifying multiple font files and having
glyphs missing from one be read from the next, but the code to do that seems
far hairier than I care to tackle.

Tonya Souther
10 Oct 2016
