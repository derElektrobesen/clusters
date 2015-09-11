bin_PROGRAMS = speedtest
speedtest_SOURCES = main.c

NODES_COUNT=8
JOBFILE=$(top_srcdir)/$(bin_PROGRAMS).job

all-local: $(top_srcdir)/small.job $(top_srcdir)/gen_job.sh $(JOBFILE)

$(JOBFILE):
	$(top_srcdir)/gen_job.sh $(bin_PROGRAMS) $(abs_srcdir) $(NODES_COUNT)

clean-local:
	rm -f $(JOBFILE)
