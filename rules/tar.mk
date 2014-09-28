# tar.mk

TAR_TARGETS = tar tar-clean

$(PACKAGE_NAME)-$(PACKAGE_FULL_VERSION).tar.gz: 
	@$(ECHO) "   TAR $(PACKAGE_NAME)-$(PACKAGE_FULL_VERSION).tar.gz"
	@$(TAR) -czf $@ --transform 's:^:$(PACKAGE_NAME)-$(PACKAGE_FULL_VERSION)/:' --exclude-vcs --exclude='$(TMPDIR)' --exclude='$@' *

tar: $(PACKAGE_NAME)-$(PACKAGE_FULL_VERSION).tar.gz

tar-clean:
	@$(ECHO) "    RM $(PACKAGE_NAME)-$(PACKAGE_FULL_VERSION).tar.gz"
	@$(RM) $(PACKAGE_NAME)-$(PACKAGE_FULL_VERSION).tar.gz
