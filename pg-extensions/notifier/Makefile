# contrib/notifier/Makefile

MODULE_big = notifier
OBJS = \
	$(WIN32RES) \
	notifier.o
PGFILEDESC = "notifier - notifications abount temporary objects creations"

TAP_TESTS = 1

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/notifier
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
