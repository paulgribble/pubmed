// dependencies: libcurl and libxml2
//
// on the mac: brew install libxml2


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>             // must have libcurl installed
#include <libxml2/libxml/xpath.h>  // must have libxml2 installed

const char *pmids_url_base = "http://eutils.ncbi.nlm.nih.gov/entrez/eutils/esearch.fcgi?db=pubmed";

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

void get_pmids(char *search_term, int retmax, char **pmid_array, int *ret, int *count){

    char retmax_str[16] = "";
    sprintf(retmax_str, "%d", retmax); 
    char pmids_url[256] = "";
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
    xmlChar *pmidspath= (xmlChar*) "//eSearchResult/IdList/Id";
    // printf("%s\n", s.ptr);
    xmlDocPtr doc = xmlParseDoc((xmlChar *)s.ptr);
    xmlXPathContextPtr context = xmlXPathNewContext(doc);
    xmlXPathObjectPtr pmids = xmlXPathEvalExpression(pmidspath, context);
    *ret = pmids->nodesetval->nodeNr;
    for (int i=0; i<*ret; i++) {
        pmid_array[i] = malloc(sizeof(char)*9);
	char *tmp = (char *)xmlNodeGetContent(pmids->nodesetval->nodeTab[i]);
        strncpy(pmid_array[i], tmp, 8);
	free(tmp);
        pmid_array[i][8] = '\0';
    }
    xmlChar *countpath = (xmlChar *)"//eSearchResult/Count";
    xmlXPathObjectPtr countPtr = xmlXPathEvalExpression(countpath, context);
    char *countChar = (char *) xmlNodeGetContent(countPtr->nodesetval->nodeTab[0]);
    *count = atoi(countChar);

    free(countChar);
    xmlXPathFreeObject(countPtr);
    xmlXPathFreeObject(pmids);
    xmlXPathFreeContext(context);
    xmlFreeDoc(doc);
    free(s.ptr);
    curl_easy_cleanup(curl);
}

char * get_xml_field(xmlChar *fieldPath, xmlXPathContextPtr context) {

  xmlXPathObjectPtr fieldPtr = xmlXPathEvalExpression(fieldPath, context);
  char *fieldStr = malloc(sizeof(char)*256);
  if (xmlXPathNodeSetIsEmpty(fieldPtr->nodesetval)) {
      fieldStr[0]='\0';
    }
    else {
      char *tmp = (char *)xmlNodeGetContent(fieldPtr->nodesetval->nodeTab[0]);
      int field_len = strlen(tmp);
      strncpy(fieldStr, tmp, field_len);
      fieldStr[field_len] = '\0';
      free(tmp);
    }
  xmlXPathFreeObject(fieldPtr);
  return fieldStr;
}

char * get_xml_authors(xmlXPathContextPtr context) {

  char *authorStr = malloc(sizeof(char)*512);
  authorStr[0] = '\0';

  xmlChar *authorListPath = (xmlChar *) "MedlineCitation/Article/AuthorList/Author";
  xmlXPathObjectPtr authors = xmlXPathEvalExpression(authorListPath, context);
  int n_authors = authors->nodesetval->nodeNr;

  xmlChar *lastnamePath = (xmlChar *) "LastName";
  xmlChar *initialsPath = (xmlChar *) "Initials";

  for (int i=0; i<n_authors; i++) {
    xmlXPathSetContextNode(authors->nodesetval->nodeTab[i], context);
    char *lastname = get_xml_field(lastnamePath, context);
    char *initials = get_xml_field(initialsPath, context);
    strcat(authorStr, lastname);
    strcat(authorStr, " ");
    strcat(authorStr, initials);
    strcat(authorStr, ", ");
    free(lastname);
    free(initials);
  }
  authorStr[strlen(authorStr)-2] = '\0';

  xmlXPathFreeObject(authors);
  return authorStr;
}

void get_articles(char **pmid_array, int ret, int do_links) {

  char fetch_url[4096]="";
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

  xmlChar *articlepath = (xmlChar *) "//PubmedArticleSet/PubmedArticle";
  xmlXPathObjectPtr articles = xmlXPathEvalExpression(articlepath, context);
  int ret_art = articles->nodesetval->nodeNr;
  //printf("%d returned\n", ret_art);

  xmlChar *yearPath = (xmlChar *) "MedlineCitation/Article/Journal/JournalIssue/PubDate/Year";
  xmlChar *titlePath = (xmlChar *) "MedlineCitation/Article/ArticleTitle";
  xmlChar *journalPath = (xmlChar *) "MedlineCitation/Article/Journal/ISOAbbreviation";
  xmlChar *volumePath = (xmlChar *) "MedlineCitation/Article/Journal/JournalIssue/Volume";
  xmlChar *issuePath = (xmlChar *) "MedlineCitation/Article/Journal/JournalIssue/Issue";  
  xmlChar *pagesPath = (xmlChar *) "MedlineCitation/Article/Pagination/MedlinePgn";
  xmlChar *doiPath = (xmlChar *) "MedlineCitation/Article/ELocationID";
  
  char citationStr[1024] = "";

  if (do_links) { printf("\n<ol reversed>\n"); }

  for (int i=0; i<ret_art; i++) {

    xmlXPathSetContextNode(articles->nodesetval->nodeTab[i], context);

    char *yearStr = get_xml_field(yearPath, context);
    char *titleStr = get_xml_field(titlePath, context);
    char *journalStr = get_xml_field(journalPath, context);
    char *volumeStr = get_xml_field(volumePath, context);
    char *issueStr = get_xml_field(issuePath, context);
    char *pagesStr = get_xml_field(pagesPath, context);
    char *doiStr = get_xml_field(doiPath, context);
    char *authorStr = get_xml_authors(context);

    strcpy(citationStr, authorStr);
    strcat(citationStr, " (");
    strcat(citationStr, yearStr);
    strcat(citationStr, ") ");
    strcat(citationStr, titleStr);
    strcat(citationStr, " ");
    if (do_links) { strcat(citationStr, "<b>"); }
    strcat(citationStr, journalStr);
    strcat(citationStr, " ");
    strcat(citationStr, volumeStr);
    if (strlen(issueStr)>0) {
      strcat(citationStr, "(");
      strcat(citationStr, issueStr);
      strcat(citationStr, ")");
    }
    if (do_links) { strcat(citationStr, "</b>"); }
    if (strlen(pagesStr)>0) {
      strcat(citationStr, ":");
      strcat(citationStr, pagesStr);
    }
    strcat(citationStr, ".");
    if (do_links) {
      if (strlen(doiStr)>0) {
        strcat(citationStr, " <a href=\"http://doi.org/");
        strcat(citationStr, doiStr);
        strcat(citationStr, "\">link</a>");
      }
      else {
        strcat(citationStr, " <a href=\"http://www.ncbi.nlm.nih.gov/pubmed/?term=");
        strcat(citationStr, pmid_array[i]);
        strcat(citationStr, "[pmid]\">link</a>");
      }
    }

    if (do_links) {
      printf("\n<li>%s</li>\n", citationStr);
    }
    else {
      printf("\n%s\n", citationStr);
    }

    free(authorStr);
    free(yearStr);
    free(titleStr);
    free(journalStr);
    free(volumeStr);
    free(issueStr);
    free(pagesStr);
    free(doiStr);

  }

  printf("\n");
  if (do_links) { printf("</ol>\n"); }
  
  xmlXPathFreeObject(articles);
  xmlXPathFreeContext(context);
  xmlFree(doc);
  curl_easy_cleanup(curl);  
}

void get_the_time(char timestr[]) {

  time_t rawtime;
  struct tm *timeinfo;

  time(&rawtime);
  timeinfo = localtime(&rawtime);
  strcpy(timestr, asctime(timeinfo));
  
}

int main(int argc, char *argv[]) {

    if (argc < 2) {
        printf("\nusage: pubmed <searchterm> <maxret> <includehtml>\n"
               "where <searchterm> is like 'gribble pl[au]'\n"
               "and <retmax> is max number of returned records\n"
               "and <includehtml> is 0 (false) or 1 (true)\n");
        return 1;
    }
    else {

      int retmax = 3;
      int do_links = 0;
      if (argc == 3) { retmax = atoi(argv[2]); }
      if (argc == 4) { retmax = atoi(argv[2]); do_links = atoi(argv[3]); }

      char **pmid_array = malloc(retmax * sizeof(char *));
      int ret = 0;
      int count = 0;

      if (do_links) {
        printf("<html>\n<head>\n");
        printf("<style>\nli {\nmargin-bottom: 1em;\n}\n</style>\n");
        printf("</head>\n<body>\n<p>");
      }

      char current_time[256];
      get_the_time(current_time);
      printf("\n%s", current_time);
      if (do_links) { printf("<br>"); }
      printf("searched: %s\n", argv[1]);
      if (do_links) { printf("<br>"); }

      for (int i=0; i<strlen(argv[1]); i++) {
        if (argv[1][i] == ' ') {
          argv[1][i] = '+';
        }
      }

      get_pmids(argv[1], retmax, pmid_array, &ret, &count);

      printf("returned %d/%d\n", ret, count);
      if (do_links) { printf("</p>\n"); }

      get_articles(pmid_array, ret, do_links);

      if (do_links) { printf("\n</body>\n</html>"); }

      for (int i=0; i<ret; i++) { free(pmid_array[i]); }
      free(pmid_array);
    }
    return 0;
}
