# rpm.mk

RPM_TARGETS = rpm rpm-tmp rpm-clean

$(TMPDIR)/SOURCES/$(PACKAGE_NAME)-$(PACKAGE_FULL_VERSION).tar.gz: $(PACKAGE_NAME)-$(PACKAGE_FULL_VERSION).tar.gz
	@$(CP) $? $@

$(PACKAGE_NAME)-$(PACKAGE_FULL_VERSION).$(ARCHITECTURE).rpm: $(TMPDIR)/SOURCES/$(PACKAGE_NAME)-$(PACKAGE_FULL_VERSION).tar.gz
	@$(ECHO) "   RPM $@"
	@rpmbuild --quiet -bb --target="$(ARCHITECTURE)"  \
	    --define "_topdir $(TMPDIR)"                  \
	    --define "package_name $(PACKAGE_NAME)"       \
	    --define "package_version $(PACKAGE_VERSION)" \
	    --define "package_release $(PACKAGE_RELEASE)" \
	    contrib/$(PACKAGE_NAME).spec > /dev/null 2>&1
	@$(CP) $(TMPDIR)/RPMS/$(ARCHITECTURE)/$@ .

rpm: rpm-tmp $(PACKAGE_NAME)-$(PACKAGE_FULL_VERSION).$(ARCHITECTURE).rpm

rpm-tmp: | $(TMPDIR)/BUILD $(TMPDIR)/BUILDROOT $(TMPDIR)/RPMS $(TMPDIR)/SPECS $(TMPDIR)/SOURCES

rpm-clean:
	@$(ECHO) "    RM $(PACKAGE_NAME)-$(PACKAGE_FULL_VERSION).$(ARCHITECTURE).rpm"
	@$(RM) $(PACKAGE_NAME)-$(PACKAGE_FULL_VERSION).$(ARCHITECTURE).rpm
