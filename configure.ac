AC_INIT([clusters], [0.1], [pberejnoy2005@gmail.com], [clusters], [https://github.com/derElektrobesen/clusters])
AM_INIT_AUTOMAKE

m4_include(configure.ac.acxmpi)

AC_SUBST([AM_CFLAGS])
AC_SUBST([AM_LDFLAGS])

ACX_MPI
AC_PROG_CC
CC=$MPICC

AC_CONFIG_FILES([
     Makefile
     speedtest/Makefile
])

AC_OUTPUT
