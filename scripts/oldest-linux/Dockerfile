# The oldest Linux the release's Linux downloads are built on, and so the
# oldest they run on.  A binary needs the glibc it was linked against or a
# newer one, never an older one, so building here is what lets the AppImages
# and the terminal tarball start on Ubuntu 18.04, Debian 10, RHEL 8 and
# everything after them -- glibc 2.27.  ubuntu-22.04, the oldest runner GitHub
# still has, would put the floor at 2.35.
#
# Not used with a workflow's `container:` key: GitHub's own actions are
# JavaScript, and the Node they bring needs glibc 2.28, so checkout, cache and
# upload-artifact all fail inside this image.  The job runs on the runner as
# usual and scripts/in-oldest-linux runs the build steps in here.
#
# Only the toolchain and the development headers.  The libraries that end up
# inside an AppImage are built from source rather than taken from 18.04, which
# would ship 2018's: Tcl and Tk by scripts/build-tcltk, SDL2 and its three
# companions by scripts/build-sdl2.  The X11, ncurses and audio headers here
# are for interfaces that have not changed in that time.
FROM ubuntu:18.04

ENV DEBIAN_FRONTEND=noninteractive

# build-essential is GCC 7.5.  CMake comes from Kitware below, not from here.
#
#   libncursesw5-dev, libtinfo-dev,  the static archives GCU_STATIC_CURSES
#   libgpm-dev                       links; 18.04's ncurses archive calls GPM
#   libx11-dev ... libudev-dev       SDL2's X11, OpenGL, audio and input
#                                    backends, which it loads at run time from
#                                    the player's system
#   libxft-dev, libfontconfig1-dev   Tk
#   zip                              Tk's build (scripts/build-tcltk says why)
#   xvfb, xauth                      a display for the Tk tests
#   python3                          scripts/smoke-tty
RUN apt-get update && apt-get install -y --no-install-recommends \
		build-essential pkg-config ninja-build git file curl \
		ca-certificates python3 \
		libncursesw5-dev libtinfo-dev libgpm-dev \
		libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev \
		libxss-dev libxxf86vm-dev libxfixes-dev libxkbcommon-dev \
		libgl1-mesa-dev libegl1-mesa-dev \
		libasound2-dev libpulse-dev libdbus-1-dev libudev-dev \
		libxft-dev libfontconfig1-dev zip xvfb xauth \
	&& rm -rf /var/lib/apt/lists/*

# 18.04's CMake is 3.10, which the game's 3.7 minimum would allow, but the
# workflows say `cmake -S . -B build` and `cmake --build build --parallel`, and
# those arrived in 3.13 and 3.12.  Kitware's own build runs on glibc 2.17, so
# its current 3.x does instead of rewriting every one of them for here.
ARG CMAKE_VERSION=3.31.12
ARG CMAKE_SHA256=0dc2e9a6860f06bf10bd8fadc03e35d9eeb4df46e33763a7e480e987758f385c
RUN curl -fsSL -o /tmp/cmake.tar.gz \
		https://github.com/Kitware/CMake/releases/download/v$CMAKE_VERSION/cmake-$CMAKE_VERSION-linux-x86_64.tar.gz \
	&& echo "$CMAKE_SHA256  /tmp/cmake.tar.gz" | sha256sum -c - \
	&& tar -xzf /tmp/cmake.tar.gz -C /opt \
	&& mv /opt/cmake-$CMAKE_VERSION-linux-x86_64 /opt/cmake && rm /tmp/cmake.tar.gz \
	&& ln -s /opt/cmake/bin/cmake /opt/cmake/bin/ctest /usr/local/bin/ \
	&& cmake --version

# The checkout belongs to the runner's user and the build runs as root, and
# git refuses to read a repository someone else owns.  scripts/version.sh
# needs it to.
RUN git config --system --add safe.directory '*'

# linuxdeploy, unpacked, so it needs no FUSE inside a container.  Its tools are
# statically linked, so the newest release runs on 18.04 as well as anywhere.
#
# The three bytes after the ELF magic are the AppImage's own marker.  The
# kernel ignores them, but the binfmt entries that run x86-64 under emulation
# on an arm64 machine match them as zero and refuse the file, so they are
# cleared before it is run.  On an x86-64 runner that changes nothing.
RUN curl -fsSL -o /tmp/linuxdeploy \
		https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage \
	&& printf '\0\0\0' | dd of=/tmp/linuxdeploy bs=1 seek=8 count=3 conv=notrunc 2>/dev/null \
	&& chmod a+x /tmp/linuxdeploy \
	&& cd /opt && /tmp/linuxdeploy --appimage-extract >/dev/null \
	&& mv squashfs-root linuxdeploy && rm /tmp/linuxdeploy \
	&& ln -s /opt/linuxdeploy/usr/bin/linuxdeploy /usr/local/bin/linuxdeploy \
	&& linuxdeploy --version
