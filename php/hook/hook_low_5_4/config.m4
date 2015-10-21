

AC_DEFUN([MYSQL_LIB_CHK], [
  str="$MYSQL_DIR/$1/lib$MYSQL_LIBNAME.*"
  for j in `echo $str`; do
    if test -r $j; then
      MYSQL_LIB_DIR=$MYSQL_DIR/$1
      break 2
    fi
  done
])

PHP_ARG_ENABLE(apm, whether to enable apm support,
[  --enable-apm            Enable apm support], yes)


PHP_ARG_WITH(mysql, enable support for MySQL,
[  --with-mysql=DIR        Location of MySQL base directory], yes, no)


PHP_ARG_ENABLE(socket, enable support for socket,
[  --enable-socket         Enable socket support], yes, no)

PHP_ARG_WITH(debugfile, enable the debug file,
[  --with-debugfile=[FILE]   Location of debugging file (/tmp/apm.debug by default)], no, no)

PHP_ARG_WITH(defaultdb, set default sqlite3 default DB path,
[  --with-defaultdb=DIR    Location of sqlite3 default DB], no, no)

if test -z "$PHP_ZLIB_DIR"; then
  PHP_ARG_WITH(zlib-dir, for the location of libz, 
  [  --with-zlib-dir[=DIR]     MySQL: Set the path to libz install prefix], no, no)
fi

if test "$PHP_APM" != "no"; then

  AC_CONFIG_HEADERS()

  if test "$PHP_DEBUGFILE" != "no"; then
    if test "$PHP_DEBUGFILE" = "yes"; then
      debugfile="/tmp/apm.debug"
    else
      debugfile="$PHP_DEBUGFILE"
    fi
    AC_DEFINE_UNQUOTED(APM_DEBUGFILE, "$debugfile", [file used for debugging APM])
  fi


  if test "$PHP_MYSQL" != "no"; then
    mysql_driver="driver_mysql.c"
    AC_DEFINE(APM_DRIVER_MYSQL, 1, [activate MySQL storage driver])

    MYSQL_DIR=
    MYSQL_INC_DIR=

    for i in $PHP_MYSQL /usr/local /usr; do
      if test -r $i/include/mysql/mysql.h; then
        MYSQL_DIR=$i
        MYSQL_INC_DIR=$i/include/mysql
        break
      elif test -r $i/include/mysql.h; then
        MYSQL_DIR=$i
        MYSQL_INC_DIR=$i/include
        break
      fi
    done

    if test -z "$MYSQL_DIR"; then
      AC_MSG_ERROR([Cannot find MySQL header files])
    fi

    if test "$enable_maintainer_zts" = "yes"; then
      MYSQL_LIBNAME=mysqlclient_r
    else
      MYSQL_LIBNAME=mysqlclient
    fi
    case $host_alias in
      *netware*[)]
        MYSQL_LIBNAME=mysql
        ;;
    esac

    if test -z "$MYSQL_LIB_DIR"; then
       MYSQL_LIB_CHK(lib/x86_64-linux-gnu)
    fi
    if test -z "$MYSQL_LIB_DIR"; then
      MYSQL_LIB_CHK(lib/i386-linux-gnu)
    fi

    for i in $PHP_LIBDIR $PHP_LIBDIR/mysql; do
      MYSQL_LIB_CHK($i)
    done

    if test -z "$MYSQL_LIB_DIR"; then
      AC_MSG_ERROR([Cannot find lib$MYSQL_LIBNAME under $MYSQL_DIR.])
    fi

    PHP_CHECK_LIBRARY($MYSQL_LIBNAME, mysql_close, [ ],
    [
      if test "$PHP_ZLIB_DIR" != "no"; then
        PHP_ADD_LIBRARY_WITH_PATH(z, $PHP_ZLIB_DIR, APM_SHARED_LIBADD)
        PHP_CHECK_LIBRARY($MYSQL_LIBNAME, mysql_error, [], [
          AC_MSG_ERROR([mysql configure failed. Please check config.log for more information.])
        ], [
          -L$PHP_ZLIB_DIR/$PHP_LIBDIR -L$MYSQL_LIB_DIR 
        ])  
        MYSQL_LIBS="-L$PHP_ZLIB_DIR/$PHP_LIBDIR -lz"
      else
        PHP_ADD_LIBRARY(z,, APM_SHARED_LIBADD)
        PHP_CHECK_LIBRARY($MYSQL_LIBNAME, mysql_errno, [], [
          AC_MSG_ERROR([Try adding --with-zlib-dir=<DIR>. Please check config.log for more information.])
        ], [
          -L$MYSQL_LIB_DIR
        ])   
        MYSQL_LIBS="-lz"
      fi
    ], [
      -L$MYSQL_LIB_DIR 
    ])

    PHP_ADD_LIBRARY_WITH_PATH($MYSQL_LIBNAME, $MYSQL_LIB_DIR, APM_SHARED_LIBADD)
    PHP_ADD_INCLUDE($MYSQL_INC_DIR)

    MYSQL_MODULE_TYPE=external
    MYSQL_LIBS="-L$MYSQL_LIB_DIR -l$MYSQL_LIBNAME $MYSQL_LIBS"
    MYSQL_INCLUDE=-I$MYSQL_INC_DIR

    PHP_SUBST_OLD(MYSQL_MODULE_TYPE)
    PHP_SUBST_OLD(MYSQL_LIBS)
    PHP_SUBST_OLD(MYSQL_INCLUDE)

    AC_DEFINE(HAVE_MYSQL,1,[MySQL found and included])
  fi


  if test "$PHP_SOCKET" != "no"; then
    socket_driver="driver_socket.c"
    AC_DEFINE(APM_DRIVER_SOCKET, 1, [activate socket driver])
  fi




  PHP_TRACE_COMMON_FILES="\
    common/trace_comm.c \
    common/trace_ctrl.c \
    common/trace_mmap.c \
    common/trace_type.c \
    common/sds/sds.c"

  PHP_NEW_EXTENSION(apm, apm.c backtrace.c log.c $PHP_TRACE_COMMON_FILES $mysql_driver  $socket_driver, $ext_shared)

  dnl configure can't use ".." as a source filename, so we make a link here
  ln -sf ./common $ext_srcdir

  dnl add common include path
  PHP_ADD_INCLUDE($ext_srcdir)

  PHP_ADD_MAKEFILE_FRAGMENT

  PHP_SUBST(APM_SHARED_LIBADD)
fi
