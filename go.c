
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>             // must have libcurl installed
#include <libxml2/libxml/xpath.h>  // must have libxml2 installed


typedef struct {
  char *ptr;
  size_t len;
} mystring;

void init_string(mystring *s) {
  s->len = 0;
  s->ptr = malloc(s->len+1);
  if (s->ptr == NULL) {
    fprintf(stderr, "malloc() failed\n");
    exit(EXIT_FAILURE);
  }
  s->ptr[0] = '\0';
}

size_t writefunc(void *ptr, size_t size, size_t nmemb, mystring *s)
{
  size_t new_len = s->len + size*nmemb;
  s->ptr = realloc(s->ptr, new_len+1);
  if (s->ptr == NULL) {
    fprintf(stderr, "realloc() failed\n");
    exit(EXIT_FAILURE);
  }
  memcpy(s->ptr+s->len, ptr, size*nmemb);
  s->ptr[new_len] = '\0';
  s->len = new_len;
  return size*nmemb;
}

/*
void get_pmids(char const *search_term, int const retmax, char **pmid_array, int *ret) {

    char retmax_str[16];
    sprintf(retmax_str, "%d", retmax); 
    char pmids_url[256];
    strcat(pmids_url, pmids_url_base);
    strcat(pmids_url, "&retmax=");
    strcat(pmids_url, retmax_str);        
    strcat(pmids_url, "&term=");
    strcat(pmids_url, search_term);

    const xmlChar *pmidspath= (xmlChar*)"//eSearchResult/IdList/Id";
    xmlDocPtr doc = xmlParseDoc((xmlChar *)s.ptr);
    xmlXPathContextPtr context = xmlXPathNewContext(doc);
    xmlXPathObjectPtr pmids = xmlXPathEvalExpression(pmidspath, context);
    *ret = pmids->nodesetval->nodeNr;
    for (int i=0; i<*ret; i++) {
        pmid_array[i] = malloc(sizeof(char)*9);
        strncpy(pmid_array[i], (char *)xmlNodeGetContent(pmids->nodesetval->nodeTab[i]), 8);
        pmid_array[i][8] = '\0';
    }
    xmlXPathFreeObject(pmids);
    xmlXPathFreeContext(context);
    xmlFreeDoc(doc);
    curl_easy_cleanup(curl);
}
*/


int main(int argc, char *argv[]) {

  char fetch_url[256]="";
  strcat(fetch_url, "http://eutils.ncbi.nlm.nih.gov/entrez/eutils/efetch.fcgi?db=pubmed&retmode=xml&id=");
  for (int i=1; i<argc; i++) {
    strcat(fetch_url, argv[i]);
    strcat(fetch_url, ",");
  }
  // printf("%s\n", fetch_url);

  CURL *curl = curl_easy_init();
  mystring s;
  init_string(&s);
  curl_easy_setopt(curl, CURLOPT_URL, fetch_url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);
  curl_easy_perform(curl);

  FILE *fid = fopen("out", "w");
  fprintf(fid, "%s\n", fetch_url);
  fprintf(fid, "%s\n", s.ptr);
  fclose(fid);

  xmlDocPtr doc = xmlParseDoc((xmlChar *)s.ptr);
  xmlXPathContextPtr context = xmlXPathNewContext(doc);

  const xmlChar *articlepath = (xmlChar *) "//PubmedArticleSet/PubmedArticle";
  xmlXPathObjectPtr articles = xmlXPathEvalExpression(articlepath, context);
  int ret = articles->nodesetval->nodeNr;
  // printf("%d returned\n", ret);

  const xmlChar *yearPath = (xmlChar *) "//PubmedArticleSet/PubmedArticle/MedlineCitation/Article/Journal/JournalIssue/PubDate/Year";
  xmlXPathObjectPtr yearPtr = xmlXPathEvalExpression(yearPath, context);

  const xmlChar *titlePath = (xmlChar *) "//PubmedArticleSet/PubmedArticle/MedlineCitation/Article/ArticleTitle";
  xmlXPathObjectPtr titlePtr = xmlXPathEvalExpression(titlePath, context);

  const xmlChar *journalPath = (xmlChar *) "//PubmedArticleSet/PubmedArticle/MedlineCitation/Article/Journal/ISOAbbreviation";
  xmlXPathObjectPtr journalPtr = xmlXPathEvalExpression(journalPath, context);

  const xmlChar *volumePath = (xmlChar *) "//PubmedArticleSet/PubmedArticle/MedlineCitation/Article/Journal/JournalIssue/Volume";
  xmlXPathObjectPtr volumePtr = xmlXPathEvalExpression(volumePath, context);

  const xmlChar *issuePath = (xmlChar *) "//PubmedArticleSet/PubmedArticle/MedlineCitation/Article/Journal/JournalIssue/Issue";
  xmlXPathObjectPtr issuePtr = xmlXPathEvalExpression(issuePath, context);

  const xmlChar *pagesPath = (xmlChar *) "//PubmedArticleSet/PubmedArticle/MedlineCitation/Article/Pagination/MedlinePgn";
  xmlXPathObjectPtr pagesPtr = xmlXPathEvalExpression(pagesPath, context);

  char citationStr[1024];

  for (int i=0; i<ret; i++) {

    char yearStr[5];
    strncpy(yearStr, (char *)xmlNodeGetContent(yearPtr->nodesetval->nodeTab[i]), 4);
    yearStr[4] = '\0';

    char titleStr[256];
    if (xmlXPathNodeSetIsEmpty(titlePtr->nodesetval)) {
      titleStr[0]='\0';
    }
    else {
      int title_len = strlen((char *)xmlNodeGetContent(titlePtr->nodesetval->nodeTab[i]));
      strncpy(titleStr, (char *)xmlNodeGetContent(titlePtr->nodesetval->nodeTab[i]), title_len);
      titleStr[title_len] = '\0';
    }

    char journalStr[256];
    if (xmlXPathNodeSetIsEmpty(journalPtr->nodesetval)) {
      journalStr[0]='\0';
    }
    else {
      int journal_len = strlen((char *)xmlNodeGetContent(journalPtr->nodesetval->nodeTab[i]));
      strncpy(journalStr, (char *)xmlNodeGetContent(journalPtr->nodesetval->nodeTab[i]), journal_len);
      journalStr[journal_len] = '\0';
    }

    char volumeStr[256];
    if (xmlXPathNodeSetIsEmpty(volumePtr->nodesetval)) {
      volumeStr[0]='\0';
    }
    else {
      int volume_len = strlen((char *)xmlNodeGetContent(volumePtr->nodesetval->nodeTab[i]));
      strncpy(volumeStr, (char *)xmlNodeGetContent(volumePtr->nodesetval->nodeTab[i]), volume_len);
      volumeStr[volume_len] = '\0';
    }
    char issueStr[256];
    if (xmlXPathNodeSetIsEmpty(issuePtr->nodesetval)) {
      issueStr[0]='\0';
    }
    else {
      int issue_len = strlen((char *)xmlNodeGetContent(issuePtr->nodesetval->nodeTab[i]));
      strncpy(issueStr, (char *)xmlNodeGetContent(issuePtr->nodesetval->nodeTab[i]), issue_len);
      issueStr[issue_len] = '\0';
  }

    char pagesStr[256];
    if (xmlXPathNodeSetIsEmpty(pagesPtr->nodesetval)) {
      pagesStr[0]='\0';
    }
    else {
      int pages_len = strlen((char *)xmlNodeGetContent(pagesPtr->nodesetval->nodeTab[i]));
      strncpy(pagesStr, (char *)xmlNodeGetContent(pagesPtr->nodesetval->nodeTab[i]), pages_len);
      pagesStr[pages_len] = '\0';
    }

    strcpy(citationStr, "(");
    strcat(citationStr, yearStr);
    strcat(citationStr, ") ");
    strcat(citationStr, titleStr);
    strcat(citationStr, " ");
    strcat(citationStr, journalStr);
    strcat(citationStr, " ");
    strcat(citationStr, volumeStr);
    strcat(citationStr, "(");
    strcat(citationStr, issueStr);
    strcat(citationStr, ")");
    strcat(citationStr, ":");
    strcat(citationStr, pagesStr);

    printf("\n%s\n", citationStr);

  }
  printf("\n");
  return 0;
}














