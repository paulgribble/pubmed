
pubmed: pubmed.c
	gcc --std=c99 pubmed.c -lxml2 -lcurl -I/usr/local/opt/libxml2/include/libxml2 -o pubmed
