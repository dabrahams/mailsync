#!/bin/sh
aclocal -I acinclude     && \
autoheader               && \
automake -a              && \
autoconf                 && \
./configure "$@"
