ALL: cap2rpc rpc2amb amb2pcm pcm2stdout xrf2amb dpl2amb


cap2rpc: cap2rpc.c cap2rpc.h dstk.h multicast.h helpandusage.h Makefile
	 gcc -Wall -lpcap -o cap2rpc cap2rpc.c

rpc2amb: rpc2amb.c rpc2amb.h dstk.h multicast.h helpandusage.h Makefile
	 gcc -Wall -lrt -o rpc2amb rpc2amb.c

amb2pcm: amb2pcm.c amb2pcm.h dstk.h multicast.h helpandusage.h serialframe.h d_initdongle.h d_ambein.h d_serialsend.h d_serialreceive.h d_pcmout.h Makefile
	gcc -Wall -lrt -o amb2pcm amb2pcm.c

pcm2stdout: pcm2stdout.c pcm2stdout.h dstk.h multicast.h helpandusage.h Makefile
	gcc -Wall -lrt -o pcm2stdout pcm2stdout.c

xrf2amb: xrf2amb.c xrf2amb.h dstk.h multicast.h helpandusage.h dextra.h Makefile
	gcc -Wall -lpthread -o xrf2amb xrf2amb.c

dpl2amb: dpl2amb.c dpl2amb.h dstk.h multicast.h helpandusage.h dplus.h Makefile
	gcc -Wall -lpthread -o dpl2amb dpl2amb.c


# Installing: cap2rpc needs setuid priviledges to be able to
# capture from an ethernet interface
install:
	install -o root -g root -m 4755 cap2rpc /usr/bin/
	install -o root -g root -m 755 rpc2amb /usr/bin/
	install -o root -g root -m 755 amb2pcm /usr/bin/
	install -o root -g root -m 755 pcm2stdout /usr/bin/
	install -o root -g root -m 755 xrf2amb /usr/bin/
	install -o root -g root -m 755 dpl2amb /usr/bin/


