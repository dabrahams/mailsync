# compiling with g++-3 worked for me with the commented ## lines - tpo

# settings for RedHat 8.0
# if you use this make sure you uncomment the relevant lines further down
# C = /usr/include/imap
# #CCLIENTLIB = /usr/lib/libc-client.so
# CFLAGS = -g  -O2 -Wall -I$(C) -fno-operator-names
# LDFLAGS = -L/usr/kerberos/lib -lc-client -lm -lssl -lpam -lgssapi_krb5

# path to c-client headers
C = /usr/include/c-client
#C = ../../libs/imap-2002.RC5/c-client/
##C = /usr/include/c-client -I/usr/include/g++-v3/

# path to c-client library
# linking dynamically
CCLIENTLIB = /usr/lib/libc-client.so
# linkging statically
#CCLIENTLIB = ../imap-2001a/c-client/c-client.a

# compiler
CC = g++
##CC = g++-3.0

# flags for your compiler
CFLAGS = -g  -O2 -Wall -I$(C)

# required libraries
#LDFLAGS = -lm -lssl
# if you are linking against c-client 2002 and
# have kerberos enabled
LDFLAGS = -lm -lssl -lgssapi_krb5
# if your system requires pam to access crypt() you have to link pam in
#LDFLAGS = -lm -lssl -lpam

default: mailsync

mailsync: mailsync.o $(CCLIENTLIB)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f mailsync mailsync.o core
