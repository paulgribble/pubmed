### on linux (tested on debian):
# install libcurl:
#   sudo apt-get install libcurl4-gnutls-dev
#
# install libxml2:
#   sudo apt-get install libxml2-dev
#
### on Mac OS X (tested on El Capitan)
# brew install libxml2
# brew link libxml2 --force

pubmed: pubmed.c
	gcc --std=c99 -Wall pubmed.c -lxml2 -lcurl -I/usr/include/libxml2 -o pubmed
