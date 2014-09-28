# defaults.mk

PACKAGE_NAME := adsmpipe

PACKAGE_MAJOR := 0
PACKAGE_MINOR := 1
PACKAGE_PATCH := 0

PACKAGE_RELEASE := 1
PACKAGE_VERSION := $(PACKAGE_MAJOR).$(PACKAGE_MINOR).$(PACKAGE_PATCH)

PACKAGE_FULL_VERSION = $(PACKAGE_VERSION)-$(PACKAGE_RELEASE)

ARCHITECTURE ?= $(shell uname -m)

DESTDIR ?=
PREFIX  ?= /usr/local

BINDIR = $(DESTDIR)$(PREFIX)/bin
LIBDIR = $(DESTDIR)$(PREFIX)/lib
SHRDIR = $(DESTDIR)$(PREFIX)/share
MANDIR = $(DESTDIR)$(PREFIX)/share/man

SRCDIR = $(shell pwd)
TMPDIR = $(SRCDIR)/.build
RULES  = $(SRCDIR)/rules

ECHO    = echo
GZIP    = gzip -c
INSTALL = install
MKDIR   = mkdir -p
TAR     = tar
RM      = rm -rf
MV      = mv
CP      = cp

