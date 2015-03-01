
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
        for (int i=0; i<ret; i++) { printf("%s\n", pmid_array[i]); }

        for (int i=0; i<ret; i++) { free(pmid_array[i]); }
        free(pmid_array);
    }
    return 0;
}
