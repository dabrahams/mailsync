#!/bin/sh
( automake --version | head -1 | grep -q ' 1\.[678]') || (echo "automake 1.6 or above is required"; exit 1)
aclocal -I acinclude     && \
autoheader               && \
automake -a              && \
autoconf                 && \
./configure "$@"
