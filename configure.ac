AC_INIT([clusters], [0.1], [pberejnoy2005@gmail.com], [clusters], [https://github.com/derElektrobesen/clusters])
AM_INIT_AUTOMAKE

m4_include(m4/configure.ac.acxmpi)
m4_include(m4/configure.ac.axcheckcompileflags)

#AC_SUBST([AM_CFLAGS])
#AC_SUBST([AM_LDFLAGS])

ACX_MPI
AC_PROG_CC

AC_CHECK_LIB([pthread], [pthread_create], [], [ AC_MSG_ERROR([ pthread library is required ]) ])
AC_CHECK_HEADERS(pthread.h)

EXTRA_FLAGS=
AX_CHECK_COMPILE_FLAG([-Wall], [ EXTRA_FLAGS="$EXTRA_FLAGS -Wall" ])
AX_CHECK_COMPILE_FLAG([-Werror], [ EXTRA_FLAGS="$EXTRA_FLAGS -Werror" ])
AX_CHECK_COMPILE_FLAG([-Wpedantic], [ EXTRA_FLAGS="$EXTRA_FLAGS -Wpedantic" ])
AX_CHECK_COMPILE_FLAG([-flto], [ EXTRA_FLAGS="$EXTRA_FLAGS -flto" ])
AX_CHECK_COMPILE_FLAG([-fpic], [ EXTRA_FLAGS="$EXTRA_FLAGS -fpic" ])

AC_SUBST(EXTRA_FLAGS)

CC=$MPICC

AC_CONFIG_FILES([
     Makefile
     speedtest/Makefile
])

AC_OUTPUT
