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
if [ ! -f cygpackage.cc ]; then
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
cd libgetopt++; ./bootstrap.sh

if test -n "$NOCONFIGURE"; then
	echo "Skipping configure per request"
	exit 0
fi

cd "$builddir"

build=`$srcdir/cfgaux/config.guess`
host="i686-pc-mingw32"

if hash $host-g++ 2> /dev/null; then
	CC="$host-gcc"
	CXX="$host-g++"
else
	CC="gcc-3 -mno-cygwin"
	CXX="g++-3 -mno-cygwin"
fi

echo "running configure"
$srcdir/configure -C --enable-maintainer-mode \
	--build=$build --host=$host CC="$CC" CXX="$CXX" \
	"$@"

exit $?
