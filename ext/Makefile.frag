# The public headers live in include/ and install into
# $(phpincludedir)/ext/swow/, so an extension writes
# #include <ext/swow/swow_shared_buffer_abi.h>

install-swow-headers:
	@$(mkinstalldirs) $(INSTALL_ROOT)$(phpincludedir)/ext/swow
	@echo "Installing swow header files:     $(INSTALL_ROOT)$(phpincludedir)/ext/swow/"
	@$(INSTALL_DATA) $(srcdir)/include/*.h $(INSTALL_ROOT)$(phpincludedir)/ext/swow/

install-headers: install-swow-headers
