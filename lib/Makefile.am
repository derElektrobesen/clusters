include $(top_srcdir)/common.am
noinst_LIBRARIES = libutils.a

LIB_TNT = $(LIB_TNT_PATH)/tnt/libtarantool.a

libutils_a_SOURCES =	utils.c			\
			tuple_space.c

libutils_a_CFLAGS =	$(AM_CFLAGS)

libutils_a_LIBADD =	$(LIB_TNT)

all-local: $(LIB_TNT)

$(LIB_TNT):
	cd $(LIB_TNT_PATH) && cmake -DCMAKE_C_COMPILER=$(CC) -DCMAKE_CXX_COMPILER=$(CXX) . && make -j 10
