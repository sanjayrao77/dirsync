# dirsync

## Description

I wanted to sync files from a Windows computer to a Linux computer, in
near-realtime. I had about 80,000 files and didn't want to run rsync on all
of that every 10 seconds.

This program will keep the file tree stats in memory, so when a change happens,
it doesn't have to scan the whole directory.

The client can check every few seconds and ask the server if there've been any
changes since the last check. Most of the time there is nothing to do, so the client finishes
immediately.

## License

This software is licensed under GPLv2. Further information is in every source
file.

## Building

Currently, only Linux and 64-bit Windows are supported. No extra libraries are required for Linux.

First, download all the files.

For the Windows server, you'll want to edit winmain.c and set the "param" and "password" variables.
The Linux server can be configured on the command line.

On Linux systems, you should be able to just run "make". That should create a server and client.

On 64-bit Windows, you need to have MinGW-w64 installed. I use the cygwin environment, where
x64\_64-w64-mingw32-gcc.exe is available. With cygwin and mingw, you can
run "make -f Makefile.mingw". This should create a server that runs with its own window.
You can create a command line client with "make -f Makefile.mingw dirsync\_client.exe".

By default, the server will poll for changes on disk every 10 seconds. This can be changed
by editing scandelay\_seconds in main.c. Hopefully, your computer will keep this directory
information in cache, so the polling won't be expensive.

## Recycledir and deletes

The client won't delete files. Instead, it will move removed files to another directory, the recycledir. This recycledir *must* be on the
same filesystem as the main file tree, so that a rename can be done instead of a copy.

## Quick start for Linux

You can do this on Windows too but the instructions are longer.

First, download the files then run "make." Hopefully you'll have "dirsync\_server" and "dirsync\_client" binaries.

In one terminal, run the server to watch /tmp/myfiles with password "MYPASSWORD":
```bash
./dirsync_server watch /tmp/myfiles MYPASSWORD
```

In another terminal, run the client to check the connection (topstat command):
```bash
./dirsync_client --syncdir=/tmp/clientdir --recycledir=/tmp/recycledir --password=MYPASSWORD --server=192.168.1.8:4000 topstat
```

To run the client sync once:
```bash
./dirsync_client --syncdir=/tmp/clientdir --recycledir=/tmp/recycledir --password=MYPASSWORD --server=192.168.1.8:4000 --interactive --progress
```

To run the client in a loop (do a dry run first!):
```bash
./dirsync_client --syncdir=/tmp/clientdir --recycledir=/tmp/recycledir --password=MYPASSWORD --server=192.168.1.8:4000 --loop
```

## Quick start for Windows server

Download the files and edit winmain.c. You can change "param" to the directory tree and "password" to your password.

After a "make -f Makefile.mingw", you should be able to run "dirsync\_server.exe" and see a status window.

## Server command line options

You can do a "dirsync\_server --help" for usage examples.

Currently, it's:
```
Usage: ./dirsync print (file|dir)
Usage: ./dirsync watch dir password
```

## Client command line options

Some options can be set in a config file. See --config below.

### listall

This will list all remote files. This could take some time.

```bash
./dirsync_client --syncdir=/tmp/clientdir --recycledir=/tmp/recycledir --password=MYPASSWORD --server=192.168.1.8:4000 listall
```

### topstat

This shows the server's status.

```bash
./dirsync_client --syncdir=/tmp/clientdir --recycledir=/tmp/recycledir --password=MYPASSWORD --server=192.168.1.8:4000 topstat
```

### --interactive

Assume an interactive console. Ask for confirmation before transferring files, etc. This is good option for testing.

```bash
./dirsync_client --syncdir=/tmp/clientdir --recycledir=/tmp/recycledir --password=MYPASSWORD --server=192.168.1.8:4000 --interactive
```

### --quiet

Show less output.

### --progress

Show file transfer progress. Good for slower interactive links.

### --all

Assume the answer is Yes to every interactive question. This is dangerous.

The questions are: new file creation, file deletion, new directory creation, directory deletion,
file updates and file verification.


### --fork

Tells the client to fork to the background.

### --loop

Tells the client to keep running. The loop frequency will depend on the server. It will wait until the server checks its
own filesystem again.

### --oneloop

Tells the client to keep running, but only long enough that all updated files have been verified.

### --config=FILENAME

This loads some settings from a file.

E.g. config.txt:
```
--syncdir=/tmp/clientdir
--recycledir=/tmp/recycledir
--password=MYPASSWORD
--server=192.168.1.8:4000
```

```bash
./dirsync_client --config=config.txt topstat
```

### --server=IPv4:PORT

This tells the client the IP address and port of the server. Only 32bit IPv4 addresses are supported.

```bash
./dirsync_client --syncdir=/tmp/clientdir --recycledir=/tmp/recycledir --password=MYPASSWORD --server=192.168.1.8:4000 --interactive
```


### --password=PASSWORD

This tells the client the password for the server. Only the first 12 bytes are used.

### --syncdir=DIRNAME

This tells the client the base of the local file tree.

### --recycledir=DIRNAME

The client won't delete files. Instead, it will move removed files to another directory, the recycledir. This recycledir *must* be on the
same filesystem as the main file tree, so that a rename can be done instead of a copy.


### --fetchdelay=MILLISECONDS

This tells the client how long to wait between network commands. If you want to reduce the load on the server,
you can increase this. The default is 0.

### --updatetime=SECONDS

This is the shared key between the server and the client. The server will monotonically increase this number as updates happen.

If the client is up-to-date at time A, and the server has its last change as A, then the server and client are still in sync.

The client starts with a time of 0, indicating it is potentially not in sync. After a sync, the server will give the client
a new updatetime to use for subsequent connections.

### --verifyall

This tells the client to do an audit, checking all files for data matching.

### --md5verify

This tells the client to use md5 to verify file data. Without this, the client will transfer the whole file again to
verify it. MD5 isn't perfect for this so it's only worth using with large files or slow links.
