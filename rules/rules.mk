# rules.mk

include $(RULES)/tmp.mk
include $(RULES)/tar.mk
include $(RULES)/rpm.mk
include $(RULES)/deb.mk

STANDARD_TARGETS = all clean install

.PHONY: $(STANDARD_TARGETS) $(TAR_TARGETS) $(RPM_TARGETS) $(DEB_TARGETS)

ADSMPIPE_OUTPUT  := $(PACKAGE_NAME)
ADSMPIPE_OBJECTS := $(patsubst %.c,%.o,$(wildcard source/*.c))

%.o: %.c
	@$(ECHO) "    CC $@"
	@$(CC) $(CFLAGS) -c $< -o $@

$(ADSMPIPE_OUTPUT): $(ADSMPIPE_OBJECTS)
	@$(ECHO) "    LD $@"
	@$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(PACKAGE_NAME).1.gz: contrib/$(PACKAGE_NAME).1
	@$(ECHO) "  GZIP $@"
	@$(GZIP) $? > $@

all: $(ADSMPIPE_OUTPUT) $(PACKAGE_NAME).1.gz

build-clean:
	@$(ECHO) "    RM $(ADSMPIPE_OBJECTS) $(ADSMPIPE_OUTPUT) $(PACKAGE_NAME).1.gz"
	@$(RM) $(ADSMPIPE_OBJECTS) $(ADSMPIPE_OUTPUT) $(PACKAGE_NAME).1.gz

distclean: clean

clean: build-clean tmp-clean tar-clean rpm-clean deb-clean

install: all
	@$(ECHO) "    INSTALL $(ADSMPIPE_OUTPUT)"
	@$(INSTALL) -D -m 0755 $(ADSMPIPE_OUTPUT) $(BINDIR)/$(ADSMPIPE_OUTPUT)
	@$(ECHO) "    INSTALL $(PACKAGE_NAME).1.gz"
	@$(INSTALL) -D -m 0644 $(PACKAGE_NAME).1.gz $(MANDIR)/man1/$(PACKAGE_NAME).1.gz
	@$(ECHO) "    INSTALL mysql.backup.sh mysql.restore.sh"
	@$(INSTALL) -D -m 0755 contrib/mysql.backup.sh $(SHRDIR)/adsmpipe/mysql.backup.sh
	@$(INSTALL) -D -m 0755 contrib/mysql.restore.sh $(SHRDIR)/adsmpipe/mysql.restore.sh

