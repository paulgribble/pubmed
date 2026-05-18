// pubmed: query NCBI PubMed E-utilities and print citations.
//
// Dependencies: libcurl, libxml2.
//   macOS:  brew install libxml2 pkg-config
//           export PKG_CONFIG_PATH="$(brew --prefix libxml2)/lib/pkgconfig"
//   Debian: apt-get install libcurl4-openssl-dev libxml2-dev pkg-config

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>
#include <libxml/xpath.h>

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

static void die(const char *msg) {
  perror(msg);
  exit(EXIT_FAILURE);
}

static void buf_init(Buffer *b) {
  b->data = malloc(1);
  if (!b->data) die("malloc");
  b->data[0] = '\0';
  b->len = 0;
  b->cap = 1;
}

static void buf_append(Buffer *b, const char *s, size_t n) {
  if (b->len + n + 1 > b->cap) {
    while (b->len + n + 1 > b->cap) b->cap *= 2;
    char *np = realloc(b->data, b->cap);
    if (!np) die("realloc");
    b->data = np;
  }
  memcpy(b->data + b->len, s, n);
  b->len += n;
  b->data[b->len] = '\0';
}

static void buf_append_str(Buffer *b, const char *s) {
  buf_append(b, s, strlen(s));
}

// Append `s` with &, <, >, " escaped for HTML.
static void buf_append_html(Buffer *b, const char *s) {
  for (const char *p = s; *p; p++) {
    switch (*p) {
      case '&': buf_append_str(b, "&amp;");  break;
      case '<': buf_append_str(b, "&lt;");   break;
      case '>': buf_append_str(b, "&gt;");   break;
      case '"': buf_append_str(b, "&quot;"); break;
      default:  buf_append(b, p, 1);         break;
    }
  }
}

static void buf_append_maybe_html(Buffer *b, const char *s, int html) {
  if (html) buf_append_html(b, s);
  else      buf_append_str(b, s);
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
  char *out = NULL;
  if (obj && !xmlXPathNodeSetIsEmpty(obj->nodesetval)) {
    xmlChar *content = xmlNodeGetContent(obj->nodesetval->nodeTab[0]);
    out = strdup(content ? (const char *)content : "");
    xmlFree(content);
  } else {
    out = strdup("");
  }
  if (!out) die("strdup");
  if (obj) xmlXPathFreeObject(obj);
  return out;
}

// Build the esearch URL for `term` with `retmax`. Caller frees. NULL on error.
static char *build_esearch_url(const char *term, int retmax) {
  CURL *esc = curl_easy_init();
  if (!esc) {
    fprintf(stderr, "curl_easy_init failed\n");
    return NULL;
  }
  char *encoded = curl_easy_escape(esc, term, 0);
  if (!encoded) {
    fprintf(stderr, "curl_easy_escape failed\n");
    curl_easy_cleanup(esc);
    return NULL;
  }
  size_t url_len = strlen(ESEARCH_URL) + strlen(encoded) + 64;
  char *url = malloc(url_len);
  if (!url) die("malloc");
  snprintf(url, url_len, "%s?db=pubmed&retmax=%d&term=%s",
           ESEARCH_URL, retmax, encoded);
  curl_free(encoded);
  curl_easy_cleanup(esc);
  return url;
}

// NULL-terminated array of PMID strings. *count gets the total match count,
// *got the number of IDs returned. Caller frees the array with free_pmid_array.
static char **get_pmids(const char *search_term, int retmax,
                        int *count, int *got) {
  *count = 0;
  *got = 0;

  char *url = build_esearch_url(search_term, retmax);
  if (!url) return NULL;
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
  if (!ctx) {
    fprintf(stderr, "xmlXPathNewContext failed\n");
    xmlFreeDoc(doc);
    return NULL;
  }

  xmlXPathObjectPtr ids = xmlXPathEvalExpression(
    (const xmlChar *)"//eSearchResult/IdList/Id", ctx);
  int n = (ids && ids->nodesetval) ? ids->nodesetval->nodeNr : 0;

  char **pmids = malloc(sizeof(char *) * (n + 1));
  if (!pmids) die("malloc");
  for (int i = 0; i < n; i++) {
    xmlChar *id = xmlNodeGetContent(ids->nodesetval->nodeTab[i]);
    pmids[i] = strdup((const char *)id);
    if (!pmids[i]) die("strdup");
    xmlFree(id);
  }
  pmids[n] = NULL;
  *got = n;

  char *cnt = xpath_text(ctx, "//eSearchResult/Count");
  *count = atoi(cnt);
  free(cnt);

  if (ids) xmlXPathFreeObject(ids);
  xmlXPathFreeContext(ctx);
  xmlFreeDoc(doc);
  return pmids;
}

static void free_pmid_array(char **pmids) {
  if (!pmids) return;
  for (int i = 0; pmids[i]; i++) free(pmids[i]);
  free(pmids);
}

// "Lastname IN, Lastname2 IN2" — malloc'd. Falls back to CollectiveName for
// group authors. Skips empty entries so no stray ", ," appears.
// Saves/restores the XPath context node so callers aren't affected.
static char *get_authors(xmlXPathContextPtr ctx) {
  xmlNodePtr saved = ctx->node;
  xmlXPathObjectPtr list = xmlXPathEvalExpression(
    (const xmlChar *)"MedlineCitation/Article/AuthorList/Author", ctx);
  int n = (list && list->nodesetval) ? list->nodesetval->nodeNr : 0;

  Buffer b;
  buf_init(&b);
  int written = 0;
  for (int i = 0; i < n; i++) {
    xmlXPathSetContextNode(list->nodesetval->nodeTab[i], ctx);
    char *last     = xpath_text(ctx, "LastName");
    char *initials = xpath_text(ctx, "Initials");
    char *group    = (*last) ? NULL : xpath_text(ctx, "CollectiveName");

    if (*last || (group && *group)) {
      if (written) buf_append_str(&b, ", ");
      if (*last) {
        buf_append_str(&b, last);
        if (*initials) {
          buf_append_str(&b, " ");
          buf_append_str(&b, initials);
        }
      } else {
        buf_append_str(&b, group);
      }
      written++;
    }
    free(last);
    free(initials);
    free(group);
  }
  if (list) xmlXPathFreeObject(list);
  ctx->node = saved;
  return b.data;
}

// Year string, malloc'd. Falls back to the leading 4 chars of MedlineDate when
// the structured Year element is missing.
static char *get_year(xmlXPathContextPtr ctx) {
  char *year = xpath_text(ctx, "MedlineCitation/Article/Journal/JournalIssue/PubDate/Year");
  if (*year) return year;
  free(year);
  char *md = xpath_text(ctx, "MedlineCitation/Article/Journal/JournalIssue/PubDate/MedlineDate");
  if (strlen(md) >= 4) md[4] = '\0';
  return md;
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

// Assemble one citation line into `out`. `out` must be initialised by caller.
static void format_citation(Buffer *out, const char *authors, const char *year,
                            const char *title, const char *journal,
                            const char *volume, const char *issue,
                            const char *pages, const char *link, int html) {
  buf_append_maybe_html(out, authors, html);
  buf_append_str(out, " (");
  buf_append_maybe_html(out, year, html);
  buf_append_str(out, ") ");
  buf_append_maybe_html(out, title, html);
  buf_append_str(out, " ");
  if (html) buf_append_str(out, "<b>");
  buf_append_maybe_html(out, journal, html);
  buf_append_str(out, " ");
  buf_append_maybe_html(out, volume, html);
  if (*issue) {
    buf_append_str(out, "(");
    buf_append_maybe_html(out, issue, html);
    buf_append_str(out, ")");
  }
  if (html) buf_append_str(out, "</b>");
  if (*pages) {
    buf_append_str(out, ":");
    buf_append_maybe_html(out, pages, html);
  }
  buf_append_str(out, ". ");
  if (html) {
    buf_append_str(out, "<a href=\"");
    buf_append_html(out, link);
    buf_append_str(out, "\">");
    buf_append_html(out, link);
    buf_append_str(out, "</a>");
  } else {
    buf_append_str(out, link);
  }
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
  if (!ctx) {
    fprintf(stderr, "xmlXPathNewContext failed\n");
    xmlFreeDoc(doc);
    return;
  }

  xmlXPathObjectPtr articles = xmlXPathEvalExpression(
    (const xmlChar *)"//PubmedArticleSet/PubmedArticle", ctx);
  int n_art = (articles && articles->nodesetval)
              ? articles->nodesetval->nodeNr : 0;

  if (html) printf("\n<ol reversed>\n");

  for (int i = 0; i < n_art; i++) {
    xmlXPathSetContextNode(articles->nodesetval->nodeTab[i], ctx);

    char *authors = get_authors(ctx);
    char *year    = get_year(ctx);
    char *title   = xpath_text(ctx, "MedlineCitation/Article/ArticleTitle");
    char *journal = xpath_text(ctx, "MedlineCitation/MedlineJournalInfo/MedlineTA");
    char *volume  = xpath_text(ctx, "MedlineCitation/Article/Journal/JournalIssue/Volume");
    char *issue   = xpath_text(ctx, "MedlineCitation/Article/Journal/JournalIssue/Issue");
    char *pages   = xpath_text(ctx, "MedlineCitation/Article/Pagination/MedlinePgn");
    char *doi     = xpath_text(ctx,
      "MedlineCitation/Article/ELocationID[@EIdType='doi']"
      " | PubmedData/ArticleIdList/ArticleId[@IdType='doi']");

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

    Buffer c;
    buf_init(&c);
    format_citation(&c, authors, year, title, journal, volume, issue, pages,
                    link.data, html);

    if (html) printf("\n<li>%s</li>\n", c.data);
    else      printf("\n%s\n", c.data);

    free(c.data);
    free(link.data);
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

  int count = 0, got = 0;
  char **pmids = get_pmids(search, retmax, &count, &got);

  printf("returned %d/%d\n", got, count);
  if (html) printf("</p>\n");

  if (got > 0) print_articles(pmids, html);
  free_pmid_array(pmids);

  if (html) printf("\n</body>\n</html>\n");
  return 0;
}
