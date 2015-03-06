CFLAGS = -std=c99 -Wall -O3  `curl-config --cflags` -I/usr/include/libxml2
LDLIBS=`curl-config --libs ` -lxml2
CC=gcc
pubmed:
go:
