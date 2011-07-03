gcc -Wall -lpcap -o cap2rpc cap2rpc.c
gcc -Wall -lrt -o rpc2amb rpc2amb.c
gcc -Wall -lrt -o amb2pcm amb2pcm.c
gcc -Wall -lrt -o pcm2stdout pcm2stdout.c
#gcc -Wall -lpthread -o xrf2amb xrf2amb.c
#gcc -Wall -lpthread -o ref2amb ref2amb.c

