#!/bin/sh

# needed for Gentoo to choose the right automake
WANT_AUTOMAKE=1.8
export WANT_AUTOMAKE

( automake --version | head -n 1 | grep -q ' 1\.[678]') || (echo "automake 1.6 or above is required"; exit 1)
aclocal -I acinclude     && \
autoheader               && \
automake -a              && \
autoconf                 && \
./configure "$@"
