packetsystem: packetsystem.o
	gcc -o packetsystem -g -lm -Wall packetsystem.o -lpthread
	#add -lnsl -lrt -D_POSIX_PTHREAD_SEMANTICS for solaris 

packetsystem.o: packetsystem.c
	gcc -g -c -Wall packetsystem.c -lpthread -D_POSIX_PTHREAD_SEMANTICS


clean:
	rm -f *.o packetsystem

