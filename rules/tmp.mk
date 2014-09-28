# tmp.mk

TMP_TARGETS = tmp tmp-clean

$(TMPDIR): ;
$(TMPDIR)/%: | $(TMPDIR)
	@$(MKDIR) $? $@

tmp: $(TMPDIR)

tmp-clean:
	@$(ECHO) "    RM $(TMPDIR) "
	@$(RM) $(TMPDIR)

