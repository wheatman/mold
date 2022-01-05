PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
LIBDIR ?= $(PREFIX)/lib
MANDIR ?= $(PREFIX)/share/man

D = $(DESTDIR)

ifeq ($(origin CC), default)
  CC = clang
endif

ifeq ($(origin CXX), default)
  CXX = clang++
endif

STRIP ?= strip

OS ?= $(shell uname -s)

# Used for both C and C++
COMMON_FLAGS = -pthread -fPIE -fno-unwind-tables -fno-asynchronous-unwind-tables -g -fsanitize=cilk

CFLAGS ?= -O2
CFLAGS += $(COMMON_FLAGS)

CXXFLAGS ?= -O2
CXXFLAGS += $(COMMON_FLAGS) -std=c++20 -fno-exceptions
CPPFLAGS += -DMOLD_VERSION=\"1.0.0\" -DLIBDIR="\"$(LIBDIR)\""
LIBS = -pthread -lz -lxxhash -ldl -lm

SRCS=$(wildcard *.cc elf/*.cc macho/*.cc)
HEADERS=$(wildcard *.h elf/*.h macho/*.h)
OBJS=$(SRCS:%.cc=out/%.o)

DEBUG ?= 0
LTO ?= 0
ASAN ?= 0
TSAN ?= 0

GIT_HASH ?= $(shell [ -d .git ] && git rev-parse HEAD)
ifneq ($(GIT_HASH),)
  CPPFLAGS += -DGIT_HASH=\"$(GIT_HASH)\"
endif

ifeq ($(DEBUG), 1)
  CXXFLAGS += -O0 -g
endif

ifeq ($(LTO), 1)
  CXXFLAGS += -flto -O3
  LDFLAGS  += -flto
endif

ifeq ($(ASAN), 1)
  CXXFLAGS += -fsanitize=address
  LDFLAGS  += -fsanitize=address
else ifeq ($(TSAN), 1)
  CXXFLAGS += -fsanitize=thread
  LDFLAGS  += -fsanitize=thread
else ifneq ($(OS), Darwin)
  # By default, we want to use mimalloc as a memory allocator.
  # Since replacing the standard malloc is not compatible with ASAN,
  # we do that only when ASAN is not enabled.
  ifdef SYSTEM_MIMALLOC
    LIBS += -lmimalloc
  else
    MIMALLOC_LIB = out/mimalloc/libmimalloc.a
    CPPFLAGS += -Ithird-party/mimalloc/include
    LIBS += -Wl,-whole-archive $(MIMALLOC_LIB) -Wl,-no-whole-archive
  endif
endif

# Homebrew on macOS/ARM installs packages under /opt/homebrew
# instead of /usr/local
ifneq ($(wildcard /opt/homebrew/.),)
  CPPFLAGS += -I/opt/homebrew/include
  LIBS += -L/opt/homebrew/lib
endif


CPPFLAGS += -Ithird-party/ParallelTools -DCILK=1
CPPFLAGS += -fopencilk

ifneq ($(OS), Darwin)
  LIBS += -lcrypto
endif

all: mold mold-wrapper.so

mold: $(OBJS) $(MIMALLOC_LIB) 
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) $(OBJS) -o $@ $(LIBS)
	ln -sf mold ld
	ln -sf mold ld64.mold

mold-wrapper.so: elf/mold-wrapper.c Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared -o $@ $(LDFLAGS) $< -ldl

out/%.o: %.cc $(HEADERS) Makefile out/elf/.keep out/macho/.keep
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

out/elf/.keep:
	mkdir -p out/elf
	touch $@

out/macho/.keep:
	mkdir -p out/macho
	touch $@

$(MIMALLOC_LIB):
	mkdir -p out/mimalloc
	(cd out/mimalloc; CFLAGS=-DMI_USE_ENVIRON=0 cmake -G'Unix Makefiles' ../../third-party/mimalloc)
	$(MAKE) -C out/mimalloc mimalloc-static


ifeq ($(OS), Darwin)
test tests check: all
	$(MAKE) -C test -f Makefile.darwin --no-print-directory
else
test tests check: all
	$(MAKE) -C test -f Makefile.linux --no-print-directory --output-sync
endif

install: all
	install -m 755 -d $D$(BINDIR)
	install -m 755 mold $D$(BINDIR)
	$(STRIP) $D$(BINDIR)/mold

	install -m 755 -d $D$(LIBDIR)/mold
	install -m 644 mold-wrapper.so $D$(LIBDIR)/mold
	$(STRIP) $D$(LIBDIR)/mold/mold-wrapper.so

	install -m 755 -d $D$(MANDIR)/man1
	install -m 644 docs/mold.1 $D$(MANDIR)/man1

	ln -sf mold $D$(BINDIR)/ld.mold
	ln -sf mold $D$(BINDIR)/ld64.mold

uninstall:
	rm -f $D$(BINDIR)/mold $D$(BINDIR)/ld.mold $D$(BINDIR)/ld64.mold
	rm -f $D$(MANDIR)/man1/mold.1
	rm -rf $D$(LIBDIR)/mold

clean:
	rm -rf *~ mold mold-wrapper.so out ld ld64.mold

.PHONY: all test tests check clean
