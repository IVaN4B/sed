# Ultimate Makefile v1.0 (C) 2016 Ivan Chebykin
# NOTE: GNU Make only

# Variables--------------------------------------------------------------------
PROJECT=sed
TESTSCRIPT=tests/test

CC=gcc
CFLAGS=-g -Wall -m64 #-DDEBUG
LDFLAGS=

OBJEXT=o
SRCEXT=c

SRCDIR=src
BUILDDIR=build
OBJDIR=obj

OBJPATH=$(BUILDDIR)/$(OBJDIR)
TARGET=$(BUILDDIR)/$(PROJECT)

SOURCES=$(shell find $(SRCDIR) -type f -name "*.$(SRCEXT)")
OBJECTS=$(patsubst $(SRCDIR)/%,$(OBJPATH)/%,$(SOURCES:.$(SRCEXT)=.$(OBJEXT)))

# Targets----------------------------------------------------------------------
all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(BUILDDIR)/$(OBJDIR)
	$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)

$(OBJPATH)/%.$(OBJEXT): $(SRCDIR)/%.$(SRCEXT)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILDDIR)

cleanobj:
	rm -rf $(OBJPATH)

remake: clean all

run: $(TARGET)
	./$(TARGET) $(ARGS)

test: $(TARGET)
	./$(TESTSCRIPT)

.PHONY: all clean cleanobj remake run
