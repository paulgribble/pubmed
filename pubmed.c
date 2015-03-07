
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>             // must have libcurl installed
#include <libxml2/libxml/xpath.h>  // must have libxml2 installed

char *pmids_url_base = "http://eutils.ncbi.nlm.nih.gov/entrez/eutils/esearch.fcgi?db=pubmed";

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

void get_pmids(char const *search_term, int const retmax, char **pmid_array, int *ret){

    char retmax_str[16];
    sprintf(retmax_str, "%d", retmax); 
    char pmids_url[256];
    strcat(pmids_url, pmids_url_base);
    strcat(pmids_url, "&retmax=");
    strcat(pmids_url, retmax_str);        
    strcat(pmids_url, "&term=");
    strcat(pmids_url, search_term);
    CURL *curl = curl_easy_init();
    if(!curl) { printf("oops! problem with curl init\n"); return; }
    mystring s;
    init_string(&s);
    curl_easy_setopt(curl, CURLOPT_URL, pmids_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);
    CURLcode res = curl_easy_perform(curl);
    if (res) {printf("oops! problem with curl download\n"); return ; }
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

void get_articles(char **pmid_array, int ret) {

  char fetch_url[2048]="";
  strcat(fetch_url, "http://eutils.ncbi.nlm.nih.gov/entrez/eutils/efetch.fcgi?db=pubmed&retmode=xml&id=");
  for (int i=0; i<ret; i++) {
    strcat(fetch_url, pmid_array[i]);
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

  // FILE *fid = fopen("out", "w");
  // fprintf(fid, "%s\n", fetch_url);
  // fprintf(fid, "%s\n", s.ptr);
  // fclose(fid);

  xmlDocPtr doc = xmlParseDoc((xmlChar *)s.ptr);
  xmlXPathContextPtr context = xmlXPathNewContext(doc);

  const xmlChar *articlepath = (xmlChar *) "//PubmedArticleSet/PubmedArticle";
  xmlXPathObjectPtr articles = xmlXPathEvalExpression(articlepath, context);
  int ret_art = articles->nodesetval->nodeNr;
  //printf("%d returned\n", ret_art);

  const xmlChar *yearPath = (xmlChar *) "MedlineCitation/Article/Journal/JournalIssue/PubDate/Year";
  const xmlChar *titlePath = (xmlChar *) "MedlineCitation/Article/ArticleTitle";
  const xmlChar *journalPath = (xmlChar *) "MedlineCitation/Article/Journal/ISOAbbreviation";
  const xmlChar *volumePath = (xmlChar *) "MedlineCitation/Article/Journal/JournalIssue/Volume";
  const xmlChar *issuePath = (xmlChar *) "MedlineCitation/Article/Journal/JournalIssue/Issue";  
  const xmlChar *pagesPath = (xmlChar *) "MedlineCitation/Article/Pagination/MedlinePgn";
  const xmlChar *doiPath = (xmlChar *) "MedlineCitation/Article/ELocationID";
  
  char citationStr[1024];

  for (int i=0; i<ret_art; i++) {

    xmlXPathSetContextNode(articles->nodesetval->nodeTab[i], context);
    xmlXPathObjectPtr yearPtr = xmlXPathEvalExpression(yearPath, context);
    xmlXPathObjectPtr titlePtr = xmlXPathEvalExpression(titlePath, context);
    xmlXPathObjectPtr journalPtr = xmlXPathEvalExpression(journalPath, context);
    xmlXPathObjectPtr volumePtr = xmlXPathEvalExpression(volumePath, context);
    xmlXPathObjectPtr issuePtr = xmlXPathEvalExpression(issuePath, context);
    xmlXPathObjectPtr pagesPtr = xmlXPathEvalExpression(pagesPath, context);
    xmlXPathObjectPtr doiPtr = xmlXPathEvalExpression(doiPath, context);

    char yearStr[5];
    if (xmlXPathNodeSetIsEmpty(yearPtr->nodesetval)) {
      yearStr[0]='\0';
    }
    else {
      int year_len = strlen((char *)xmlNodeGetContent(yearPtr->nodesetval->nodeTab[0]));
      strncpy(yearStr, (char *)xmlNodeGetContent(yearPtr->nodesetval->nodeTab[0]), year_len);
      yearStr[year_len] = '\0';
    }

    char titleStr[256];
    if (xmlXPathNodeSetIsEmpty(titlePtr->nodesetval)) {
      titleStr[0]='\0';
    }
    else {
      int title_len = strlen((char *)xmlNodeGetContent(titlePtr->nodesetval->nodeTab[0]));
      strncpy(titleStr, (char *)xmlNodeGetContent(titlePtr->nodesetval->nodeTab[0]), title_len);
      titleStr[title_len] = '\0';
    }

    char journalStr[256];
    if (xmlXPathNodeSetIsEmpty(journalPtr->nodesetval)) {
      journalStr[0]='\0';
    }
    else {
      int journal_len = strlen((char *)xmlNodeGetContent(journalPtr->nodesetval->nodeTab[0]));
      strncpy(journalStr, (char *)xmlNodeGetContent(journalPtr->nodesetval->nodeTab[0]), journal_len);
      journalStr[journal_len] = '\0';
    }

    char volumeStr[256];
    if (xmlXPathNodeSetIsEmpty(volumePtr->nodesetval)) {
      volumeStr[0]='\0';
    }
    else {
      int volume_len = strlen((char *)xmlNodeGetContent(volumePtr->nodesetval->nodeTab[0]));
      strncpy(volumeStr, (char *)xmlNodeGetContent(volumePtr->nodesetval->nodeTab[0]), volume_len);
      volumeStr[volume_len] = '\0';
    }

    char issueStr[256];
    if (xmlXPathNodeSetIsEmpty(issuePtr->nodesetval)) {
      issueStr[0]='\0';
    }
    else {
      int issue_len = strlen((char *)xmlNodeGetContent(issuePtr->nodesetval->nodeTab[0]));
      strncpy(issueStr, (char *)xmlNodeGetContent(issuePtr->nodesetval->nodeTab[0]), issue_len);
      issueStr[issue_len] = '\0';
    }

    char pagesStr[256];
    if (xmlXPathNodeSetIsEmpty(pagesPtr->nodesetval)) {
      pagesStr[0]='\0';
    }
    else {
      int pages_len = strlen((char *)xmlNodeGetContent(pagesPtr->nodesetval->nodeTab[0]));
      strncpy(pagesStr, (char *)xmlNodeGetContent(pagesPtr->nodesetval->nodeTab[0]), pages_len);
      pagesStr[pages_len] = '\0';
    }

    char doiStr[256];
    if (xmlXPathNodeSetIsEmpty(doiPtr->nodesetval)) {
      doiStr[0]='\0';
    }
    else {
      int doi_len = strlen((char *)xmlNodeGetContent(doiPtr->nodesetval->nodeTab[0]));
      strncpy(doiStr, (char *)xmlNodeGetContent(doiPtr->nodesetval->nodeTab[0]), doi_len);
      doiStr[doi_len] = '\0';
    }

    strcpy(citationStr, "(");
    strcat(citationStr, yearStr);
    strcat(citationStr, ") ");
    strcat(citationStr, titleStr);
    strcat(citationStr, " ");
    strcat(citationStr, journalStr);
    strcat(citationStr, " ");
    strcat(citationStr, volumeStr);
    if (strlen(issueStr)>0) {
      strcat(citationStr, "(");
      strcat(citationStr, issueStr);
      strcat(citationStr, ")");
    }
    if (strlen(pagesStr)>0) {
      strcat(citationStr, ":");
      strcat(citationStr, pagesStr);
    }
    strcat(citationStr, ".");
    if (strlen(doiStr)>0) {
      strcat(citationStr, " http://doi.org/");
      strcat(citationStr, doiStr);
    }
    
    printf("\n%s\n", citationStr);

  }
  printf("\n");

}


int main(int argc, char *argv[]) {

    int retmax = 10;
    if (argc < 2) {
        printf("\nusage: pubmed <searchterm> <maxret>\n"
               "where <searchterm> is like 'gribble+pl[au]'\n"
               "and (optional) <retmax> is max number of returned records (default = 10)\n\n");
        return 1;
    }
    else {
        if (argc == 3) { retmax = atoi(argv[2]); }

        char **pmid_array = malloc(retmax * sizeof(char *));
        int ret = 0;

        get_pmids(argv[1], retmax, pmid_array, &ret);

        printf("returned %d\n", ret);

        get_articles(pmid_array, ret);        

        for (int i=0; i<ret; i++) { free(pmid_array[i]); }
        free(pmid_array);
    }
    return 0;
}
