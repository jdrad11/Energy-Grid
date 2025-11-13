#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total = size * nmemb;
    char **response_ptr = (char **)userp;

    *response_ptr = realloc(*response_ptr, strlen(*response_ptr) + total + 1);
    if (*response_ptr == NULL) {
        fprintf(stderr, "Out of memory!\n");
        return 0;
    }

    strncat(*response_ptr, contents, total);
    return total;
}

int main(void) {
    CURL *curl;
    CURLcode res;
    char *response = calloc(1, 1);

    // JSON data to send
    const char *json_data = "{\"service\": \"C_Test_Service\", \"amount\": 25}";

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (curl) {
        // Set URL
        curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:8080/use");

        // Set POST method
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");

        // Set JSON payload
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);

        // Set headers
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Capture response
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        // Optional: set a user agent
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "barebones-c-client/1.0");

        // Perform the request
        res = curl_easy_perform(curl);

        if (res == CURLE_OK) {
            printf("Response:\n%s\n", response);
        } else {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }

        // Cleanup
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    free(response);
    curl_global_cleanup();
    return 0;
}

