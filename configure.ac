AC_INIT([clusters], [0.1], [pberejnoy2005@gmail.com], [clusters], [https://github.com/derElektrobesen/clusters])
AM_INIT_AUTOMAKE

m4_include(m4/configure.ac.acxmpi)
m4_include(m4/configure.ac.axcheckcompileflags)

AC_PROG_CC
AC_PROG_RANLIB
ACX_MPI

AC_CHECK_LIB([pthread], [pthread_create], [], [ AC_MSG_ERROR([ pthread library is required ]) ])
AC_CHECK_HEADERS(pthread.h)

EXTRA_FLAGS=
AX_CHECK_COMPILE_FLAG([-Wall], [ EXTRA_FLAGS="$EXTRA_FLAGS -Wall" ])
AX_CHECK_COMPILE_FLAG([-Werror], [ EXTRA_FLAGS="$EXTRA_FLAGS -Werror" ])
AX_CHECK_COMPILE_FLAG([-flto], [ EXTRA_FLAGS="$EXTRA_FLAGS -flto" ])
AX_CHECK_COMPILE_FLAG([-fpic], [ EXTRA_FLAGS="$EXTRA_FLAGS -fpic" ])
AX_CHECK_COMPILE_FLAG([-std=c99], [ EXTRA_FLAGS="$EXTRA_FLAGS -std=c99" ])
AX_CHECK_COMPILE_FLAG([-Wno-gnu], [ EXTRA_FLAGS="$EXTRA_FLAGS -Wno-gnu" ])
AX_CHECK_COMPILE_FLAG([-Wformat], [ EXTRA_FLAGS="$EXTRA_FLAGS -Wformat" ])

AC_SUBST(EXTRA_FLAGS)

CC=$MPICC

AC_CONFIG_FILES([
	Makefile
	lib/Makefile
	speedtest/Makefile
	tuple_space/Makefile
])

AC_OUTPUT
