# dvdnav.library: DVD-Video navigation for any Amiga program, by
# libdvdread 7.1.1 and libdvdnav 7.0.0 (VideoLAN; third_party/,
# GPL-2.0-or-later) behind a small Amiga interface of its own.
# GPL-3.0-or-later, (c) 2026 Brian King; the interface (include/,
# dvdnav_lib.sfd) is MIT.
#
#   make            build/dvdnav.library (68020 and up, no FPU)
#   make test       its tests on a small DVD image (tests/fixtures/dvd):
#                   on this computer, linked with the library's sources,
#                   then through dvdnav.library on a 68020 under vamos
#   make lha        build/dvdnav.lha, for Aminet (no source: it points to
#                   https://github.com/luvwagn/dvdnav.library)
#
# Needs AmigaPorts' m68k-amigaos-gcc 16.2 (rc24 or later; the continuation
# of bebbo's amiga-gcc), with libnix (-noixemul), a C compiler for this
# computer and Python 3 for the tests, and amitools' vamos.

CC      = m68k-amigaos-gcc
STRIP   = m68k-amigaos-strip
HOSTCC ?= cc
PYTHON ?= python3
CPU     = -m68020-60
LIB     = build/dvdnav.library
DVDREAD = third_party/libdvdread/src
DVDNAV  = third_party/libdvdnav/src
SRC     = $(addprefix $(DVDREAD)/,bitreader.c dvd_input.c dvd_reader.c dvd_udf.c ifo_read.c \
            logger.c md5.c nav_read.c) \
          $(addprefix $(DVDNAV)/,dvdnav.c highlight.c logger.c navigation.c read_cache.c \
            searching.c settings.c vm/decoder.c vm/getset.c vm/play.c vm/vm.c vm/vmcmd.c vm/vmget.c) \
          src/port.c src/dvdnavlib.c
OBJ     = $(patsubst %.c,build/obj/%.o,$(SRC))
HDR     = $(wildcard src/*.h src/port/*.h src/port/*/*.h include/libraries/*.h \
            $(DVDREAD)/*.h $(DVDREAD)/dvdread/*.h $(DVDNAV)/*.h $(DVDNAV)/dvdnav/*.h $(DVDNAV)/vm/*.h)
# the C library libdvd calls is port.c's (port/amiport.h, forced in);
# each library's own headers first, as their own builds have them
FLAGS   = -std=gnu11 -Wall -Wno-unused-function -Wno-attributes -DHAVE_CONFIG_H -Isrc/port \
          -Iinclude -Isrc -include src/port/amiport.h
LIBCC   = $(CC) -O2 $(CPU) -noixemul -nostartfiles -fomit-frame-pointer
INC     = -I$(DVDREAD) -I$(DVDNAV)
# Its Resident's version from src/dvdnavlib.c's LibVersion, so the two
# cannot differ: ramlib turns a library away whose Resident is older than
# the version asked for, whatever its lib_Version (make test checks it)
LIBVER  = $(shell sed -n 's/^const UWORD LibVersion = \([0-9]*\);.*/\1/p' src/dvdnavlib.c)
ISO     = build/test.iso

.PHONY: all test lha clean

all: $(LIB)

build/obj/$(DVDNAV)/%.o: INC = -I$(DVDNAV) -I$(DVDNAV)/vm -I$(DVDREAD)
# libdvd passes int32_t * where int * is declared (and back): the same 32
# bits on the Amiga, so its own files are built as upstream builds them
build/obj/third_party/%.o: UPSTREAM = -Wno-incompatible-pointer-types -Wno-discarded-qualifiers
build/obj/%.o: %.c $(HDR)
	@mkdir -p $(dir $@)
	@echo "  CC    $<"
	@$(LIBCC) $(FLAGS) $(UPSTREAM) $(INC) -c -o $@ $<

build/amitime.o: src/amitime.c
	@mkdir -p build
	@echo "  CC    $<"
	@$(LIBCC) -Wall -c -o $@ $<
build/libinit.o: src/libinit.c src/libinit.h src/dvdnavlib.c Makefile
	@mkdir -p build
	@echo "  CC    $<"
	@$(CC) -O2 -m68020 -noixemul -nostartfiles -fomit-frame-pointer -Wall -Wno-array-bounds \
		-Isrc -DLIB_RT_VERSION=$(LIBVER) -c -o $@ $<
$(LIB): build/libinit.o $(OBJ) build/amitime.o
	@echo "  LD    $@"
	@$(LIBCC) -o $@.dbg build/libinit.o $(OBJ) build/amitime.o -lamiga
	@$(STRIP) -o $@ $@.dbg

# ---- the tests (tests/dvd_test.c) on a DVD image made for them
# (tests/mkdvd.sh, kept xz-compressed): on the host with the library's
# sources linked in (big-endian or not, the same code), then on a 68020
# under vamos through the library itself
$(ISO): tests/fixtures/dvd/test.iso.xz
	@mkdir -p build
	@$(PYTHON) -I -c "import lzma, sys; open(sys.argv[2], 'wb').write(lzma.open(sys.argv[1]).read())" $< $@
build/host/dvd_test: tests/dvd_test.c $(SRC) $(HDR) tests/host/exec/types.h
	@mkdir -p build/host
	@echo "  CC    $@"
	@set -e; o=""; for f in $(SRC); do \
		case $$f in $(DVDNAV)/*) i="-I$(DVDNAV) -I$(DVDNAV)/vm -I$(DVDREAD)";; *) i="-I$(DVDREAD) -I$(DVDNAV)";; esac; \
		b=build/host/$$(echo $$f | tr / _ | sed 's/\.c$$/.o/'); \
		$(HOSTCC) -O2 $(FLAGS) -w -Itests/host $$i -c -o $$b $$f; o="$$o $$b"; done; \
		$(HOSTCC) -O2 -Wall -Iinclude -Itests/host -o $@ tests/dvd_test.c $$o
build/68k/dvd_test: tests/dvd_test.c include/libraries/dvdnav.h include/inline/dvdnav.h $(LIB)
	@mkdir -p build/68k/Libs
	@echo "  CC    $@"
	@$(CC) -O2 $(CPU) -noixemul -Wall -Wno-pointer-sign -Iinclude -o $@ tests/dvd_test.c -lamiga
	@cp $(LIB) build/68k/Libs/

test: build/host/dvd_test build/68k/dvd_test $(LIB) $(ISO)
	@$(PYTHON) -c "import sys; d=open('$(LIB)','rb').read(); i=d.find(bytes([0x4a,0xfc])); \
		sys.exit(0 if i >= 0 and d[i+11] == $(LIBVER) else '$(LIB): its Resident says version %d, not $(LIBVER)' % d[i+11])" \
		&& echo "  CHECK the library's Resident: version $(LIBVER), as LibVersion (ramlib turns an older one away)"
	@build/host/dvd_test $(ISO) > build/host/out.txt 2>&1 && grep -q "dvd tests passed" build/host/out.txt \
		&& echo "  CHECK DVD navigation (host) ok" || { cat build/host/out.txt; exit 1; }
	@cp $(ISO) build/68k/test.iso
	@cd build/68k && vamos -C 68020 -m 8192 -s 48 -V prog:$(abspath build/68k) \
		prog:dvd_test prog:test.iso > out.txt 2>&1 && grep -q "dvd tests passed" out.txt \
		&& echo "  CHECK DVD navigation (68020, dvdnav.library) ok" || { cat out.txt; exit 1; }

# ---- for Aminet: the library, its interface and documentation; the
# source is at https://github.com/luvwagn/dvdnav.library (README.md)
lha: all
	@rm -rf build/lha && mkdir -p build/lha/dvdnav/Libs build/lha/dvdnav/Include
	@cp $(LIB) build/lha/dvdnav/Libs/
	@cp -R include/* build/lha/dvdnav/Include/
	@cp dvdnav_lib.sfd README.md LICENSE CHANGES build/lha/dvdnav/
	@cp third_party/libdvdread/COPYING build/lha/dvdnav/COPYING.libdvdread
	@cp third_party/libdvdnav/COPYING build/lha/dvdnav/COPYING.libdvdnav
	@cp dvdnav.readme build/lha/dvdnav.readme
	@cd build/lha && rm -f ../dvdnav.lha && lha aqo5 ../dvdnav.lha dvdnav dvdnav.readme
	@echo "  LHA   build/dvdnav.lha"

clean:
	rm -rf build
