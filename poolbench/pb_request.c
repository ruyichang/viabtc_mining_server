# include <curl/curl.h>
# include "pb_config.h"
# include "pb_request.h"

static nw_job *job;

struct request_context {
    char              *url;
    request_callback  callback;
};

static size_t write_callback_func(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    sds *reply = userdata;
    *reply = sdscatlen(*reply, ptr, size * nmemb);
    return size * nmemb;
}

static json_t *http_request(const char *url, double timeout)
{
    json_t *reply  = NULL;
    json_t *result = NULL;

    CURL *curl = curl_easy_init();
    sds reply_str = sdsempty();

    char auth[100];
    snprintf(auth, sizeof(auth), "%s: %s", "AUTHORIZATION", settings.request_auth);
    struct curl_slist *chunk = NULL;
    chunk = curl_slist_append(chunk, auth);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback_func);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &reply_str);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)(timeout * 1000));

    printf("http_request -- 1. \n");

    CURLcode ret = curl_easy_perform(curl);
    if (ret != CURLE_OK) {
        log_fatal("get %s fail: %s", url, curl_easy_strerror(ret));
        goto cleanup;
    }
    printf("http_request -- 2. \n");


    reply = json_loads(reply_str, 0, NULL);
    if (reply == NULL) {
        log_fatal("[json_loads]parse %s reply fail: %s", url, reply_str);
        goto cleanup;
    }

    printf("http_request -- 3. \n");

//    json_t *error;
//    error = json_object_get(reply, "error");
//    if (!error) {
//        log_fatal("[\"error\"]reply error: %s: %s", url, reply_str);
//        goto cleanup;
//    }
//
//    printf("http_request -- 4. \n");
//
//    json_t *message = json_object_get(error, "message");
//    if (!message || strcmp(json_string_value(message), "ok") != 0) {
//        log_fatal("[\"message\"]reply error: %s: %s", url, reply_str);
//        goto cleanup;
//    }
//    printf("http_request -- 5. \n");


    result = json_object_get(reply, "result");
    json_incref(result);

cleanup:
    curl_easy_cleanup(curl);
    sdsfree(reply_str);
    if (reply)
        json_decref(reply);
    curl_slist_free_all(chunk);

    return result;
}

static void on_job(nw_job_entry *entry, void *privdata)
{
    double start = current_timestamp();
    struct request_context *req = entry->request;
    entry->reply = http_request(req->url, 5.0);
    double end = current_timestamp();
    log_info("request url: %s cost: %f", req->url, end - start);
}

static void on_job_finish(nw_job_entry *entry)
{
    struct request_context *req = entry->request;
    req->callback(entry->reply);
}

static void on_job_cleanup(nw_job_entry *entry)
{
    struct request_context *req = entry->request;
    free(req);
}

int xinit_jobmaster_config(void)
{
    printf("[init_jobmaster_config]--1.\n");

    json_t *data = http_request(settings.jobmaster_url, 5.0);
    printf("[init_jobmaster_config]--2.\n");

    if (data == NULL){
        printf("[init_jobmaster_config]--3.\n");
        return -__LINE__;
    }

    if (settings.jobmaster_cfg)
        json_decref(settings.jobmaster_cfg);
    settings.jobmaster_cfg = data;
    printf("[init_jobmaster_config--]4.\n");

    return 0;
}

int update_jobmaster_config(request_callback callback)
{
    struct request_context *req = malloc(sizeof(struct request_context));
    req->url        = settings.jobmaster_url;
    req->callback   = callback;
    return nw_job_add(job, 0, req);
}

int init_request(void)
{
    nw_job_type type;
    memset(&type, 0, sizeof(type));
    type.on_job     = on_job;
    type.on_finish  = on_job_finish;
    type.on_cleanup = on_job_cleanup;

    job = nw_job_create(&type, 1);
    if (job == NULL)
        return -__LINE__;
    return 0;
}

