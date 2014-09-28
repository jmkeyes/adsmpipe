# Makefile

SUPPORTED_PLATFORMS = $(patsubst Makefile.%,%,$(wildcard Makefile.*))

ifndef PLATFORM
all::
%:
	@echo ""
	@echo "Please select a single target platform."
	@echo "  $(SUPPORTED_PLATFORMS)"
	@echo ""
	@echo "  $(MAKE) PLATFORM=<platform> $@"
	@echo ""
else
%:
	@$(MAKE) --no-print-directory -f Makefile.$(PLATFORM) $@
endif
