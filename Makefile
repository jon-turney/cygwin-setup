all: gzs setup.exe

setup.exe:
	if test -z "`type -p cmd`" ; then start /wait msdev setup.dsw /make "setup - Win32 Release"; else cmd /c start /wait msdev setup.dsw /make "setup - Win32 Release";	fi

gzs: cygwin1.dll.gz tar.exe.gz gzip.exe.gz cygpath.exe.gz mount.exe.gz

cygwin1.dll.gz: /usr/bin/cygwin1.dll
	gzip -9fc $< >$@

tar.exe.gz: /usr/bin/tar.exe
	gzip -9fc $< >$@

gzip.exe.gz: /usr/bin/gzip.exe
	gzip -9fc $< >$@

cygpath.exe.gz: /usr/bin/cygpath.exe
	gzip -9fc $< >$@

mount.exe.gz: /usr/bin/mount.exe
	gzip -9fc $< >$@

clean:
	-rm -rf Debug Release
	-rm *~ *.exe *.o *.res *.opt *.ncb *.plg

distclean: clean
	-rm *.exe.gz *.dll.gz cygwin1.dll
