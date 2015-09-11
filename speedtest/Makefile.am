bin_PROGRAMS = speedtest
speedtest_SOURCES = main.c

NODES_COUNT=8

all-local:
	../gen_job.sh '$(bin_PROGRAMS)' '$(shell pwd)' '$(NODES_COUNT)'
