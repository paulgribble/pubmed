# pubmed
C code for searching PubMed
(work in progress)

dependencies: libcurl and libxml2

## building

### macOS (Homebrew)

libcurl ships with macOS via the Xcode Command Line Tools, so only libxml2 needs to be installed:

```
xcode-select --install
brew install libxml2
brew link libxml2 --force
sudo ln -s /usr/local/include/libxml /opt/homebrew/include/libxml2/libxml/
make pubmed-mac
```

### Linux (Debian/Ubuntu)

```
sudo apt-get install libcurl4-gnutls-dev libxml2-dev
make pubmed-linux
```

usage examples:

retrieve 3 most recent articles by P.L. Gribble and print them to the screen:

```
./pubmed 'gribble pl[au]' 3
```

![](screenshot_1.png)


retrieve 5 most recent articles by P.L. Gribble, and write the list
	 to a file called gribble.html in pretty html format, and then
	 open that file in a web browser (works on a mac):

```
./pubmed 'gribble pl[au]' 5 1 > gribble.html && open gribble.html
```

![](screenshot_2.png)
