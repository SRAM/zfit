CFLAGS=-g2 -Wall -Wextra -O2 -std=c99 -I. -DFIT_ZIP_MAIN
ZNAME=fit_zip
HEATSHRINK_DIR=../heatshrink

all: $(ZNAME)

TAGS: $(ZNAME).c $(UZNAME).c $(HEATSHRINK_DIR)/*.c $(HEATSHRINK_DIR)/*.h *.h
	etags $^

$(ZNAME): $(ZNAME).o
	$(CC) -o $@ $< -lm

$(ZNAME).exe: $(ZNAME).c
	i586-mingw32msvc-cc -static $(CFLAGS) -o $@ $< -lm
	i586-mingw32msvc-strip $@

valgrind: $(ZNAME)
	valgrind --track-origins=yes ./$(ZNAME) *.fit 2>&1 | less 

clean:
	rm -f $(ZNAME).o $(ZNAME) $(ZNAME).exe fit_zip fit_zip.exe *.pyc
	rm -f verify.ps verify.ps verify.txt

verify: verify.pdf

verify.pdf: verify.ps
	ps2pdf verify.ps

verify.ps: verify.txt
	enscript -o verify.ps -f Courier8 -r -G verify.txt

verify.txt: $(ZNAME) verify.py
	python verify.py | tee verify.txt

test:
	<in.fuze ./fit_zip.py >in.fuze.diff
	<in.fuze.diff ./rle2.py >in.fuze.diff.z
	mv log.tmp log.in.tmp
	<in.fuze.diff.z ./rle2.py --decode >out.fuze.diff.z
	<out.fuze.diff.z ./fit_unzip.py >out.fuze
	mv log.tmp log.out.tmp
	ls -l in.fuze out.fuze
	cmp in.fuze out.fuze ; true
	rm -f in.fuze.bz2 in.fuze.diff.bz2
	./heatshrink < in.fuze >in.fuze.hs
	./heatshrink < in.fuze.diff > in.fuze.diff.hs
	./heatshrink < in.fuze.diff.z > in.fuze.diff.z.hs
	bzip2 -k in.fuze in.fuze.diff
	ls -l in.fuze in.fuze.diff in.fuze.bz2 in.fuze.diff.bz2 in.fuze.diff.z out.fuze.diff.z in.fuze.hs in.fuze.diff.hs in.fuze.diff.z.hs
