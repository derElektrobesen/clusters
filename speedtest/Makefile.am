#bin_PROGRAMS = speedtest
#
#include $(top_srcdir)/common.am
#
#NODES_COUNT=8
#JOBFILE=$(top_srcdir)/$(bin_PROGRAMS).job
#LIBDIR=$(top_srcdir)/lib
#
#speedtest_SOURCES =			\
#	main.c				\
#	speedtest.c
#
#speedtest_LDADD = $(LIBDIR)/libutils.a
#
#speedtest_CFLAGS =			\
#	$(AM_CFLAGS)			\
#	-DNODES_COUNT=$(NODES_COUNT)
#
#all-local: $(top_srcdir)/small.job $(top_srcdir)/gen_job.sh $(JOBFILE)
#
#$(JOBFILE):
#	$(top_srcdir)/gen_job.sh $(bin_PROGRAMS) $(abs_srcdir) $(NODES_COUNT)
#
#clean-local:
#	rm -f $(JOBFILE)
