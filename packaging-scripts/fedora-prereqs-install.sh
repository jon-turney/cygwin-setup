#!/bin/bash

sudo dnf -y install automake bison diffutils flex libtool make upx
sudo dnf -y install mingw32-gcc-c++ mingw32-zlib-static mingw32-libgcrypt-static mingw32-bzip2-static mingw32-xz-libs-static mingw32-winpthreads-static
sudo dnf -y install mingw64-gcc-c++ mingw64-zlib-static mingw64-libgcrypt-static mingw64-bzip2-static mingw64-xz-libs-static mingw64-winpthreads-static
sudo dnf -y install 'dnf-command(copr)'
sudo dnf -y copr enable jturney/mingw-libsolv
sudo dnf -y install mingw32-libsolv-static mingw64-libsolv-static
sudo dnf -y install mingw32-libgnurx-static mingw64-libgnurx-static
sudo dnf -y copr enable jturney/mingw-zstd
sudo dnf -y install mingw32-libzstd-static mingw64-libzstd-static
