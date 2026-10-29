# The two tools that turn the 3DS build's .3dsx into a .cia, a title the 3DS
# installs to its home menu, for scripts/build-3ds --cia.  They are the two
# nintendo.yaml uses, from the same places: cxitool from devkitPro's 3dstools,
# on the cxi-stuff branch it has never been merged from, and makerom from
# Project_CTR.  Neither is in the devkitARM image, and CI compiles both on
# every run; here they are compiled once, into an image that is built on
# first use and reused after that.
#
# Pinned to commits, so that the image built next year has these tools and
# not whatever the two branches have become by then.
FROM devkitpro/devkitarm

ARG TOOLS3DS_COMMIT=26a73b75d37db9c5be2e6ea172ddfc0c6f278bea
ARG PROJECT_CTR_COMMIT=d52c2b0e788c2d085763d7c5dcc4b2edd9ffb7d8

RUN apt-get update && apt-get install -y --no-install-recommends \
		build-essential automake autoconf autotools-dev git ca-certificates \
		libmbedtls-dev liblzma-dev libyaml-dev \
	&& rm -rf /var/lib/apt/lists/*

# Into a prefix of its own, with only cxitool put on the path: the branch's
# 3dsxtool and smdhtool are older than devkitPro's own and would otherwise
# shadow them.
RUN git clone -b cxi-stuff https://github.com/devkitPro/3dstools /tmp/3dstools \
	&& cd /tmp/3dstools \
	&& git checkout "$TOOLS3DS_COMMIT" \
	&& ./autogen.sh \
	&& ./configure --prefix=/opt/3dstools \
	&& make -j"$(nproc)" install \
	&& ln -s /opt/3dstools/bin/cxitool /usr/local/bin/cxitool \
	&& rm -rf /tmp/3dstools

RUN git clone https://github.com/3DSGuy/Project_CTR /tmp/Project_CTR \
	&& cd /tmp/Project_CTR/makerom \
	&& git checkout "$PROJECT_CTR_COMMIT" \
	&& make -j"$(nproc)" deps \
	&& make -j"$(nproc)" \
	&& cp bin/makerom /usr/local/bin/makerom \
	&& rm -rf /tmp/Project_CTR
