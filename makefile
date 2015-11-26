# makefile for linux (tested on debian)
#
# install libcurl:
#   sudo apt-get install libcurl4-gnutls-dev
#
# install libxml2:
#   sudo apt-get install libxml2-dev
#

pubmed: pubmed.c
	gcc --std=c99 -Wall pubmed.c -lxml2 -lcurl -I/usr/include/libxml2 -o pubmed
