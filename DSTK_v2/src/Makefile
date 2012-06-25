#ALL: cap2rpc rpc2amb amb2pcm pcm2stdout xrf2amb dpl2amb
ALL: cap2rpc rpc2amb


cap2rpc: cap2rpc.c cap2rpc.h include/dstk.h include/multicast.h include/helpandusage.h include/globaldef.h include/dss_rpc.h Makefile
	 gcc -Wall -lpcap -o cap2rpc cap2rpc.c

rpc2amb: rpc2amb.c rpc2amb.h include/dstkfunctions.h include/dss_amb.h include/dstk.h include/multicast.h include/helpandusage.h Makefile
	 gcc -Wall -lrt -o rpc2amb rpc2amb.c


# Installing: cap2rpc needs setuid priviledges to be able to
# capture from an ethernet interface
install:
	install -o root -g root -m 4755 cap2rpc /usr/bin/
	install -o root -g root -m 755 rpc2amb /usr/bin/

