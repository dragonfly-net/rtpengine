TARGET=		all-tests

with_transcoding ?= yes

CFLAGS=		-g -Wall -pthread -fno-strict-aliasing
CFLAGS+=	-std=c99
CFLAGS+=	$(shell pkg-config --cflags glib-2.0)
CFLAGS+=	$(shell pkg-config --cflags gthread-2.0)
CFLAGS+=	$(shell pkg-config --cflags openssl)
CFLAGS+=	-I. -I../lib/ -I../kernel-module/ -I../include/
CFLAGS+=	-D_GNU_SOURCE
ifeq ($(with_transcoding),yes)
CFLAGS+=	$(shell pkg-config --cflags libavcodec)
CFLAGS+=	$(shell pkg-config --cflags libavformat)
CFLAGS+=	$(shell pkg-config --cflags libavutil)
CFLAGS+=	$(shell pkg-config --cflags libswresample)
CFLAGS+=	$(shell pkg-config --cflags libavfilter)
CFLAGS+=	-DWITH_TRANSCODING
CFLAGS+=	$(shell pkg-config --cflags libpcre)
CFLAGS+=	$(shell pkg-config --cflags zlib)
CFLAGS+=	$(shell pkg-config --cflags json-glib-1.0)
CFLAGS+=	$(shell pkg-config --cflags libevent_pthreads)
CFLAGS+=	$(shell pkg-config xmlrpc_client --cflags 2> /dev/null || xmlrpc-c-config client --cflags)
CFLAGS+=	$(shell pkg-config xmlrpc --cflags 2> /dev/null)
CFLAGS+=	$(shell pkg-config xmlrpc_util --cflags 2> /dev/null)
else
CFLAGS+=	-DWITHOUT_CODECLIB
endif

LDLIBS=		-lm
LDLIBS+=	$(shell pkg-config --libs glib-2.0)
LDLIBS+=	$(shell pkg-config --libs gthread-2.0)
LDLIBS+=	$(shell pkg-config --libs libcrypto)
LDLIBS+=	$(shell pkg-config --libs openssl)
ifeq ($(with_transcoding),yes)
LDLIBS+=	$(shell pkg-config --libs libavcodec)
LDLIBS+=	$(shell pkg-config --libs libavformat)
LDLIBS+=	$(shell pkg-config --libs libavutil)
LDLIBS+=	$(shell pkg-config --libs libswresample)
LDLIBS+=	$(shell pkg-config --libs libavfilter)
LDLIBS+=	$(shell pkg-config --libs libpcre)
LDLIBS+=	$(shell pkg-config --libs zlib)
LDLIBS+=	$(shell pkg-config --libs json-glib-1.0)
LDLIBS+=	-lpcap
LDLIBS+=	$(shell pkg-config --libs libevent_pthreads)
LDLIBS+=	$(shell pkg-config xmlrpc_client --libs 2> /dev/null || xmlrpc-c-config client --libs)
LDLIBS+=	$(shell pkg-config xmlrpc --libs 2> /dev/null)
LDLIBS+=	$(shell pkg-config xmlrpc_util --libs 2> /dev/null)
LDLIBS+=	-lhiredis
endif

SRCS=		bitstr-test.c aes-crypt.c payload-tracker-test.c const_str_hash-test.strhash.c
LIBSRCS=	loglib.c auxlib.c str.c rtplib.c
DAEMONSRCS=	crypto.c
HASHSRCS=

ifeq ($(with_transcoding),yes)
SRCS+=		amr-decode-test.c amr-encode-test.c transcode-test.c
LIBSRCS+=	codeclib.c resample.c
DAEMONSRCS+=	codec.c ssrc.c call.c ice.c aux.c kernel.c media_socket.c stun.c bencode.c socket.c poller.c \
		dtls.c recording.c statistics.c rtcp.c redis.c iptables.c graphite.c \
		rtp.c streambuf.c cookie_cache.c udp_listener.c homer.c load.c cdr.c dtmf.c
HASHSRCS+=	call_interfaces.c control_ng.c sdp.c
endif

OBJS=		$(SRCS:.c=.o) $(LIBSRCS:.c=.o) $(DAEMONSRCS:.c=.o) $(HASHSRCS:.c=.strhash.o)

COMMONOBJS=	str.o auxlib.o rtplib.o loglib.o

include ../lib/common.Makefile

include		.depend

.PHONY:		all-tests unit-tests daemon-tests

TESTS=		bitstr-test aes-crypt payload-tracker-test const_str_hash-test.strhash
ifeq ($(with_transcoding),yes)
TESTS+=		amr-decode-test amr-encode-test transcode-test
endif

ADD_CLEAN=	tests-preload.so $(TESTS)

all-tests:	unit-tests daemon-tests
	true # override linking recipe from common.Makefile

unit-tests:	$(TESTS)
	for x in $(TESTS); do echo testing: $$x; G_DEBUG=fatal-warnings ./$$x || exit 1; done

daemon-tests:	tests-preload.so
	$(MAKE) -C ../daemon
	rm -rf fake-sockets
	mkdir fake-sockets
	LD_PRELOAD=../t/tests-preload.so RTPE_BIN=../daemon/rtpengine TEST_SOCKET_PATH=./fake-sockets \
		   perl -I../perl auto-daemon-tests.pl
	test "$$(ls fake-sockets)" = ""
	rmdir fake-sockets

bitstr-test:	bitstr-test.o

amr-decode-test: amr-decode-test.o $(COMMONOBJS) codeclib.o resample.o

amr-encode-test: amr-encode-test.o $(COMMONOBJS) codeclib.o resample.o

aes-crypt:	aes-crypt.o $(COMMONOBJS) crypto.o

transcode-test:	transcode-test.o $(COMMONOBJS) codeclib.o resample.o codec.o ssrc.o call.o ice.o aux.o \
	kernel.o media_socket.o stun.o bencode.o socket.o poller.o dtls.o recording.o statistics.o \
	rtcp.o redis.o iptables.o graphite.o call_interfaces.strhash.o sdp.strhash.o rtp.o crypto.o \
	control_ng.strhash.o \
	streambuf.o cookie_cache.o udp_listener.o homer.o load.o cdr.o dtmf.o

payload-tracker-test: payload-tracker-test.o $(COMMONOBJS) ssrc.o aux.o auxlib.o rtp.o crypto.o

const_str_hash-test.strhash: const_str_hash-test.strhash.o $(COMMONOBJS)

tests-preload.so:	tests-preload.c
	$(CC) -g -D_GNU_SOURCE -std=c99 -o $@ -Wall -shared -fPIC $<
