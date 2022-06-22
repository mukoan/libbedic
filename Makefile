INSTALL_DIR=/usr/local
VERSION=1.1
RELEASE=0
DASH_RELEASE=$(VERSION)-$(RELEASE)
DOT_RELEASE=$(VERSION).$(RELEASE)

ifeq ($(ARCH), arm)
    CC=arm-linux-gcc
    CXX=arm-linux-g++
    LD=arm-linux-g++
    ARCH_CXXFLAGS=-I$(QTDIR)/include -O2 -DARCH_ARM
    LIBS=-L/opt/Qtopia/sharp/lib -lz -lsqlite3
    DEBUG=1
else
    ARCH=x86
    CC=gcc
    CXX=g++
    LD=g++
    ARCH_CXXFLAGS=-O2 -DARCH_X86
    DEBUG=1
endif

OBJDIR=objs.$(ARCH)
TARGET=$(OBJDIR)/libbedic.a
COMMON_CFLAGS=-pipe -Wall -W
COMMON_CXXFLAGS=-pipe -Wall -DQWS -fno-rtti -fPIC
INCLUDES=-Iinclude
CFLAGS=$(COMMON_CFLAGS) $(ARCH_CFLAGS) $(INCLUDES)
CXXFLAGS=$(COMMON_CXXFLAGS) $(ARCH_CXXFLAGS) $(INCLUDES) -DVERSION=\"$(DOT_RELEASE)\"
LIBS+=-lz -lsqlite3

ifdef DEBUG
    CXXFLAGS+=-g
    CFLAGS+=-g
else
    CXXFLAGS+=-O2 -DNO_DEBUG
    CFLAGS+=-O2 -DNO_DEBUG
endif

SOURCES=src/shc.c src/shcm.cpp src/utf8.cpp src/dictionary_impl.cpp src/file.cpp \
     src/dynamic_dictionary.cpp src/bedic_wrapper.cpp src/dictionary_factory.cpp \
     src/hybrid_dictionary.cpp src/format_entry.cpp
OBJS=$(OBJDIR)/shc.o $(OBJDIR)/shcm.o $(OBJDIR)/utf8.o $(OBJDIR)/dictionary_impl.o $(OBJDIR)/file.o \
     $(OBJDIR)/dynamic_dictionary.o $(OBJDIR)/bedic_wrapper.o $(OBJDIR)/dictionary_factory.o \
     $(OBJDIR)/hybrid_dictionary.o $(OBJDIR)/format_entry.o

all: $(TARGET) xerox mkbedic

test_dynamic_dictionary: $(TARGET) src/test_dynamic_dictionary.cpp
	$(CXX) -o $(OBJDIR)/test_dynamic_dictionary $(CXXFLAGS) src/test_dynamic_dictionary.cpp -L$(OBJDIR) -lbedic $(LIBS)

xerox: $(TARGET) src/xerox.cpp
	echo $(LIBRARY_PATH)
	$(CXX) -o $(OBJDIR)/xerox $(CXXFLAGS) src/xerox.cpp -L$(OBJDIR) -lbedic $(LIBS)

mkbedic: $(TARGET) src/mkbedic.cpp
	echo $(LIBRARY_PATH)
	$(CXX) -o $(OBJDIR)/mkbedic $(CXXFLAGS) src/mkbedic.cpp -L$(OBJDIR) -lbedic $(LIBS)

headwords: $(TARGET) tools/headwords.cpp
	$(CXX) -o $(OBJDIR)/headwords $(CXXFLAGS) tools/headwords.cpp -L$(OBJDIR) -lbedic $(LIBS)

dist:
	tar -czf libbedic_$(VERSION)-$(RELEASE).tgz \
	  `find ./ ! -name "*~" ! -path "*CVS*" ! -name "*.ipk" ! -path "./objs*" ! -name "*.tgz" -type f ! -name ".#*"`

$(OBJDIR)/%.o: src/%.c
	$(CXX) -c -o $@ $(CXXFLAGS) $<

$(OBJDIR)/%.o: src/%.cpp
	$(CXX) -c -o $@ $(CXXFLAGS) $<

$(TARGET): $(OBJDIR) $(OBJS)
	ar cr $@ $(OBJS)

$(OBJDIR):
	@mkdir -p $@

$(OBJDIR)/dynamic_dictionary.o: src/dynamic_dictionary.cpp include/bedic.h

$(OBJDIR)/dictionary_impl.o: src/dictionary_impl.cpp src/dictionary_impl.h include/bedic.h include/dictionary.h

$(OBJDIR)/bedic_wrapper.o: src/bedic_wrapper.cpp include/bedic.h

$(OBJDIR)/dictionary_factory.o: src/dictionary_factory.cpp include/bedic.h

$(OBJDIR)/hybrid_dictionary.o: src/hybrid_dictionary.cpp src/dictionary_impl.h include/bedic.h


install:
	install objs.x86/xerox $(INSTALL_DIR)/bin
	install src/xerox.1 $(INSTALL_DIR)/man/man1/

clean:
	@rm -rf $(OBJS) $(OBJDIR)
