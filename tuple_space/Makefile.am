bin_PROGRAMS = just_test

include $(top_srcdir)/common.am

NODES_COUNT=8
JOBFILE=$(top_srcdir)/$(bin_PROGRAMS).job
LIBDIR=$(top_srcdir)/lib

just_test_SOURCES =			\
	main.c

just_test_LDADD = $(LIBDIR)/libutils.a \
		  $(top_srcdir)/tarantool-client/tnt/libtarantool.a

just_test_CFLAGS =			\
	$(AM_CFLAGS)			\
	-DNODES_COUNT=$(NODES_COUNT)	\
	-I$(top_srcdir)/tarantool-client/include \
	-I$(top_srcdir)/tarantool-client/third_party

all-local: $(top_srcdir)/small.job $(top_srcdir)/gen_job.sh $(JOBFILE)

$(JOBFILE):
	$(top_srcdir)/gen_job.sh $(bin_PROGRAMS) $(abs_srcdir) $(NODES_COUNT)

clean-local:
	rm -f $(JOBFILE)
