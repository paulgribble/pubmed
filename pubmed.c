// pubmed: query NCBI PubMed E-utilities and print citations.
//
// Dependencies: libcurl, libxml2.
//   macOS:  brew install libxml2
//   Debian: apt-get install libcurl4-gnutls-dev libxml2-dev

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>
#include <libxml2/libxml/xpath.h>

#define DEFAULT_RETMAX 3

static const char *ESEARCH_URL =
  "https://eutils.ncbi.nlm.nih.gov/entrez/eutils/esearch.fcgi";
static const char *EFETCH_URL =
  "https://eutils.ncbi.nlm.nih.gov/entrez/eutils/efetch.fcgi";

static const char HTML_HEAD[] =
  "<html>\n<head>\n"
  "<meta charset=\"UTF-8\">\n"
  "<style>\n"
  "ol { padding-left: 0%; }\n"
  "li { margin-bottom: 0.5em; }\n"
  "body {\n"
  "  font-family: verdana, sans-serif;\n"
  "  font-size: 10pt;\n"
  "  text-align: left;\n"
  "  padding-left: 5%;\n"
  "  padding-right: 5%;\n"
  "  padding-bottom: 5%;\n"
  "  padding-top: 1%;\n"
  "  line-height: 1.3;\n"
  "}\n"
  "body a { color: #2580a2; text-decoration: none; }\n"
  "</style>\n</head>\n<body>\n<p>";

// Growable byte buffer.

typedef struct {
  char *data;
  size_t len;
  size_t cap;
} Buffer;

static void buf_init(Buffer *b) {
  b->data = malloc(1);
  if (!b->data) { perror("malloc"); exit(EXIT_FAILURE); }
  b->data[0] = '\0';
  b->len = 0;
  b->cap = 1;
}

static void buf_append(Buffer *b, const char *s, size_t n) {
  if (b->len + n + 1 > b->cap) {
    while (b->len + n + 1 > b->cap) b->cap *= 2;
    char *np = realloc(b->data, b->cap);
    if (!np) { perror("realloc"); exit(EXIT_FAILURE); }
    b->data = np;
  }
  memcpy(b->data + b->len, s, n);
  b->len += n;
  b->data[b->len] = '\0';
}

static void buf_append_str(Buffer *b, const char *s) {
  buf_append(b, s, strlen(s));
}

static size_t buf_curl_cb(void *ptr, size_t size, size_t nmemb, Buffer *b) {
  size_t n = size * nmemb;
  buf_append(b, ptr, n);
  return n;
}

// HTTP GET into a malloc'd, NUL-terminated string. Caller frees. NULL on error.
static char *http_get(const char *url) {
  CURL *curl = curl_easy_init();
  if (!curl) {
    fprintf(stderr, "curl_easy_init failed\n");
    return NULL;
  }
  Buffer b;
  buf_init(&b);
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, buf_curl_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &b);
  CURLcode rc = curl_easy_perform(curl);
  curl_easy_cleanup(curl);
  if (rc != CURLE_OK) {
    fprintf(stderr, "curl error (%s): %s\n", url, curl_easy_strerror(rc));
    free(b.data);
    return NULL;
  }
  return b.data;
}

// First-match text of an XPath query, malloc'd. Empty string if no match.
static char *xpath_text(xmlXPathContextPtr ctx, const char *xpath) {
  xmlXPathObjectPtr obj = xmlXPathEvalExpression((const xmlChar *)xpath, ctx);
  char *out;
  if (!obj || xmlXPathNodeSetIsEmpty(obj->nodesetval)) {
    out = strdup("");
  } else {
    xmlChar *content = xmlNodeGetContent(obj->nodesetval->nodeTab[0]);
    out = strdup(content ? (const char *)content : "");
    xmlFree(content);
  }
  if (obj) xmlXPathFreeObject(obj);
  return out;
}

// NULL-terminated array of PMID strings. *count gets the total match count.
static char **get_pmids(const char *search_term, int retmax, int *count) {
  *count = 0;

  CURL *escaper = curl_easy_init();
  char *encoded = curl_easy_escape(escaper, search_term, 0);
  size_t url_len = strlen(ESEARCH_URL) + strlen(encoded) + 64;
  char *url = malloc(url_len);
  snprintf(url, url_len, "%s?db=pubmed&retmax=%d&term=%s",
           ESEARCH_URL, retmax, encoded);
  curl_free(encoded);
  curl_easy_cleanup(escaper);

  char *xml = http_get(url);
  free(url);
  if (!xml) return NULL;

  xmlDocPtr doc = xmlParseDoc((const xmlChar *)xml);
  free(xml);
  if (!doc) {
    fprintf(stderr, "xmlParseDoc failed for esearch response\n");
    return NULL;
  }

  xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
  xmlXPathObjectPtr ids = xmlXPathEvalExpression(
    (const xmlChar *)"//eSearchResult/IdList/Id", ctx);
  int n = (ids && ids->nodesetval) ? ids->nodesetval->nodeNr : 0;

  char **pmids = malloc(sizeof(char *) * (n + 1));
  for (int i = 0; i < n; i++) {
    xmlChar *id = xmlNodeGetContent(ids->nodesetval->nodeTab[i]);
    pmids[i] = strdup((const char *)id);
    xmlFree(id);
  }
  pmids[n] = NULL;

  char *cnt = xpath_text(ctx, "//eSearchResult/Count");
  *count = atoi(cnt);
  free(cnt);

  if (ids) xmlXPathFreeObject(ids);
  xmlXPathFreeContext(ctx);
  xmlFreeDoc(doc);
  return pmids;
}

// "Lastname IN, Lastname2 IN2" — malloc'd. Empty string if no authors.
// Saves/restores the XPath context node so callers aren't affected.
static char *get_authors(xmlXPathContextPtr ctx) {
  xmlNodePtr saved = ctx->node;
  xmlXPathObjectPtr list = xmlXPathEvalExpression(
    (const xmlChar *)"MedlineCitation/Article/AuthorList/Author", ctx);
  int n = (list && list->nodesetval) ? list->nodesetval->nodeNr : 0;

  Buffer b;
  buf_init(&b);
  for (int i = 0; i < n; i++) {
    xmlXPathSetContextNode(list->nodesetval->nodeTab[i], ctx);
    char *last = xpath_text(ctx, "LastName");
    char *initials = xpath_text(ctx, "Initials");
    if (i > 0) buf_append_str(&b, ", ");
    buf_append_str(&b, last);
    buf_append_str(&b, " ");
    buf_append_str(&b, initials);
    free(last);
    free(initials);
  }
  if (list) xmlXPathFreeObject(list);
  ctx->node = saved;
  return b.data;
}

static char *build_efetch_url(char **pmids) {
  Buffer b;
  buf_init(&b);
  buf_append_str(&b, EFETCH_URL);
  buf_append_str(&b, "?db=pubmed&retmode=xml&id=");
  for (int i = 0; pmids[i]; i++) {
    if (i > 0) buf_append_str(&b, ",");
    buf_append_str(&b, pmids[i]);
  }
  return b.data;
}

static void print_articles(char **pmids, int html) {
  char *url = build_efetch_url(pmids);
  char *xml = http_get(url);
  free(url);
  if (!xml) return;

  xmlDocPtr doc = xmlParseDoc((const xmlChar *)xml);
  free(xml);
  if (!doc) {
    fprintf(stderr, "xmlParseDoc failed for efetch response\n");
    return;
  }

  xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
  xmlXPathObjectPtr articles = xmlXPathEvalExpression(
    (const xmlChar *)"//PubmedArticleSet/PubmedArticle", ctx);
  int n_art = (articles && articles->nodesetval)
              ? articles->nodesetval->nodeNr : 0;

  if (html) printf("\n<ol reversed>\n");

  for (int i = 0; i < n_art; i++) {
    xmlXPathSetContextNode(articles->nodesetval->nodeTab[i], ctx);

    char *authors = get_authors(ctx);
    char *year    = xpath_text(ctx, "MedlineCitation/Article/Journal/JournalIssue/PubDate/Year");
    char *title   = xpath_text(ctx, "MedlineCitation/Article/ArticleTitle");
    char *journal = xpath_text(ctx, "MedlineCitation/MedlineJournalInfo/MedlineTA");
    char *volume  = xpath_text(ctx, "MedlineCitation/Article/Journal/JournalIssue/Volume");
    char *issue   = xpath_text(ctx, "MedlineCitation/Article/Journal/JournalIssue/Issue");
    char *pages   = xpath_text(ctx, "MedlineCitation/Article/Pagination/MedlinePgn");
    char *doi     = xpath_text(ctx,
      "MedlineCitation/Article/ELocationID[@EIdType='doi']"
      " | PubmedData/ArticleIdList/ArticleId[@IdType='doi']");

    Buffer c;
    buf_init(&c);
    buf_append_str(&c, authors);
    buf_append_str(&c, " (");
    buf_append_str(&c, year);
    buf_append_str(&c, ") ");
    buf_append_str(&c, title);
    buf_append_str(&c, " ");
    if (html) buf_append_str(&c, "<b>");
    buf_append_str(&c, journal);
    buf_append_str(&c, " ");
    buf_append_str(&c, volume);
    if (*issue) {
      buf_append_str(&c, "(");
      buf_append_str(&c, issue);
      buf_append_str(&c, ")");
    }
    if (html) buf_append_str(&c, "</b>");
    if (*pages) {
      buf_append_str(&c, ":");
      buf_append_str(&c, pages);
    }
    buf_append_str(&c, ".");

    Buffer link;
    buf_init(&link);
    if (*doi) {
      buf_append_str(&link, "https://doi.org/");
      buf_append_str(&link, doi);
    } else {
      buf_append_str(&link, "https://www.ncbi.nlm.nih.gov/pubmed/?term=");
      buf_append_str(&link, pmids[i]);
      buf_append_str(&link, "[pmid]");
    }
    buf_append_str(&c, " ");
    if (html) {
      buf_append_str(&c, "<a href=\"");
      buf_append_str(&c, link.data);
      buf_append_str(&c, "\">");
      buf_append_str(&c, link.data);
      buf_append_str(&c, "</a>");
    } else {
      buf_append_str(&c, link.data);
    }
    free(link.data);

    if (html) printf("\n<li>%s</li>\n", c.data);
    else      printf("\n%s\n", c.data);

    free(c.data);
    free(authors);
    free(year);
    free(title);
    free(journal);
    free(volume);
    free(issue);
    free(pages);
    free(doi);
  }

  printf("\n");
  if (html) printf("</ol>\n");

  if (articles) xmlXPathFreeObject(articles);
  xmlXPathFreeContext(ctx);
  xmlFreeDoc(doc);
}

static void free_pmid_array(char **pmids) {
  if (!pmids) return;
  for (int i = 0; pmids[i]; i++) free(pmids[i]);
  free(pmids);
}

static int pmid_count(char **pmids) {
  int n = 0;
  if (pmids) while (pmids[n]) n++;
  return n;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr,
      "\nusage: pubmed <searchterm> [<retmax>] [<htmlout>]\n"
      "  <searchterm>  PubMed query (e.g. 'gribble pl[au]')\n"
      "  <retmax>      Max records returned (default %d)\n"
      "  <htmlout>     0 for plain text (default), 1 for HTML\n\n",
      DEFAULT_RETMAX);
    return 1;
  }
  const char *search = argv[1];
  int retmax = (argc > 2) ? atoi(argv[2]) : DEFAULT_RETMAX;
  int html   = (argc > 3) ? atoi(argv[3]) : 0;

  if (html) fputs(HTML_HEAD, stdout);

  time_t now = time(NULL);
  printf("\n%s", asctime(localtime(&now)));
  if (html) printf("<br>");
  printf("searched: %s\n", search);
  if (html) printf("<br>");

  int count = 0;
  char **pmids = get_pmids(search, retmax, &count);
  int got = pmid_count(pmids);

  printf("returned %d/%d\n", got, count);
  if (html) printf("</p>\n");

  if (got > 0) print_articles(pmids, html);
  free_pmid_array(pmids);

  if (html) printf("\n</body>\n</html>\n");
  return 0;
}
