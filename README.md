# pubmed
C code for searching PubMed
(work in progress)

dependencies: libcurl and libxml2

usage examples:

retrieve 3 most recent articles by P.L. Gribble and print them to the screen:

```
./pubmed 'gribble pl[au]' 3
```

```
Sat Nov 21 09:58:56 2015
searched: gribble pl[au]
returned 3/54

Weiler J, Gribble PL, Pruszynski JA (2015) Goal-dependent modulation
of the long-latency stretch response at the shoulder, elbow and
wrist. J. Neurophysiol. :jn.00702.2015.

Martin CB, Cowell RA, Gribble PL, Wright J, Köhler S (2015)
Distributed category-specific recognition-memory signals in human
perirhinal cortex. Hippocampus .

Wood DK, Gu C, Corneil BD, Gribble PL, Goodale MA (2015) Transient
visual responses reset the phase of low-frequency oscillations in the
skeletomotor periphery. Eur. J. Neurosci. 42(3):1919-32.

```

retrieve 10 most recent articles by P.L. Gribble, and write the list
	 to a file called gribble.html in pretty html format, and then
	 open that file in a web browser (works on a mac):

```
./pubmed 'gribble pl[au]' 10 1 > gribble.html ; open gribble.html
```

[gribble.html](http://htmlpreview.github.com/?https://github.com/paulgribble/pubmed/blob/master/gribble.html)

![html preview][screenshot.png]
