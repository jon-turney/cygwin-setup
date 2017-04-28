#! /bin/sh
# Used to setup the configure.in, autoheader and Makefile.in's if configure
# has not been generated. This script is only needed for developers when
# configure has not been run, or if a Makefile.am in a non-configured directory
# has been updated

builddir=`pwd`
srcdir=`dirname "$0"`

bootstrap() {
  if "$@"; then
    true # Everything OK
  else
    echo "$1 failed"
    echo "Autotool bootstrapping failed. You will need to investigate and correct" ;
    echo "before you can develop on this source tree" 
    exit 1
  fi
}

cd "$srcdir"

# Make sure we are running in the right directory
if [ ! -f main.cc ]; then
  echo "You must run this script from the directory containing it"
  exit 1
fi


# Make sure cfgaux exists
mkdir -p cfgaux

# Bootstrap the autotool subsystems
echo "bootstrapping in $srcdir"
bootstrap aclocal
# bootstrap autoheader
bootstrap libtoolize --automake
bootstrap autoconf
bootstrap automake --foreign --add-missing

# Run bootstrap in required subdirs, iff it has not yet been run
echo "bootstrapping in $srcdir/libgetopt++"
cd libgetopt++ && ./bootstrap.sh

if test -n "$NOCONFIGURE"; then
	echo "Skipping configure per request"
	exit 0
fi

cd "$builddir"

build=`$srcdir/cfgaux/config.guess`

if hash i686-w64-mingw32-g++ 2> /dev/null; then
	host="i686-w64-mingw32"
elif hash i686-pc-mingw32-g++ 2> /dev/null; then
	host="i686-pc-mingw32"
elif hash x86_64-w64-mingw32-g++ 2> /dev/null; then
	host="x86_64-w64-mingw32"
else
	echo "mingw32 or mingw64 target g++ required for building setup"
	exit 1
fi

echo "running configure"
$srcdir/configure -C --build=$build --host=$host "$@"

exit $?
