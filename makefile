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
# sudo ln -s /usr/local/opt/libxml2/include/libxml2/libxml /usr/local/include/libxml

pubmed-mac: pubmed.c
	gcc --std=c99 -Wall pubmed.c -I/usr/local/opt/libxml2/include/libxml2 -L/usr/local/opt/libxml2/lib -lxml2 -lcurl -o pubmed

pubmed-linux:	pubmed.c
	gcc --std=c99 -Wall pubmed.c -I/usr/include/libxml2/include/libxml2 -L/usr/include/libxml2/lib -lxml2 -lcurl -o pubmed

