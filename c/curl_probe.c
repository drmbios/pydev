#include <curl/curl.h>

int main(void) {
    return curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK ? 0 : 1;
}
