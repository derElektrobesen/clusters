bin_PROGRAMS = lu_decomposition

include $(top_srcdir)/common.am

NODES_COUNT=8
JOBFILE=$(top_srcdir)/$(bin_PROGRAMS).job
LIBDIR=$(top_srcdir)/lib

lu_decomposition_SOURCES =	main.c
lu_decomposition_LDADD =	$(LIBDIR)/libutils.a			\
				$(LIB_TNT)
lu_decomposition_CFLAGS =	$(AM_CFLAGS)				\
				-DTUPLE_SPACE_HOST='"mgmt.nodes"'	\
				-DTUPLE_SPACE_PORT=10010

all-local: $(top_srcdir)/small.job $(top_srcdir)/gen_job.sh $(JOBFILE)

$(JOBFILE):
	$(top_srcdir)/gen_job.sh $(bin_PROGRAMS) $(abs_srcdir) $(NODES_COUNT)

clean-local:
	rm -f $(JOBFILE)
