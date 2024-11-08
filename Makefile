CFLAGS=-g -Wall -O2 -DLINUX
all: dirsync_server dirsync_client
dirsync_server: main.o server.o dirsync.o filebyname.o dirbyname.o command.o common/blockmem.o common/filename.o common/fileio.o common/socket.o common/netcommand.o common/someutils.o common/md5.o
	gcc -o $@ $^
netcommand_server: netcommand_server.o command.o common/socket.o common/netcommand.o common/someutils.o
	gcc -o $@ $^
dirsync_client: netcommand_client.o config.o command.o client_dirsync.o utf32.o dirsync.o filebyname.o dirbyname.o common/socket.o common/netcommand.o common/someutils.o common/filename.o common/blockmem.o common/fileio.o common/md5.o
	gcc -o $@ $^
clean:
	rm -f netcommand_server dirsync_server dirsync_client core *.o common/*.o
edition: clean
	scp -pr * 192.168.1.115:/home/guilty2/src/dirsync/
heart: clean
	scp -pr * heart:src/dirsync/
cleantest:
	rm -rf /tmp/dirtest2/*
jesus: clean
	tar -jcf - . | jesus src.dirsync.tar.bz2
