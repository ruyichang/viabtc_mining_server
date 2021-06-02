# include <curl/curl.h>
# include "bp_config.h"
# include "bp_request.h"

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

json_t* get_peer_list(const char *url){
    double timeout = 10*2;
    json_t *reply  = NULL;
    json_t *result = NULL;


    struct curl_slist *headers = NULL;
    CURLcode status;
    long code;
    sds reply_str = sdsempty();

    CURL *curl = curl_easy_init();

    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_TRY);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "curl");
    // curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout); //ms not effect
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback_func);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &reply_str);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    status = curl_easy_perform(curl);
    if (status != 0) {
        printf("unable to request data from: %s, error: %s \n", url, curl_easy_strerror(status));
        goto cleanup;
    }

    printf("reply_str:%s - len:%d\n", reply_str, sdslen(reply_str));


    reply = json_loads(reply_str, 0, NULL);
    if (reply == NULL) {
        printf("parse %s reply fail: %s", url, reply_str);
        goto cleanup;
    }



//    json_incref(reply);

    json_t *error;
    error = json_object_get(reply, "error");
    if (!error) {
        log_fatal("reply error: %s: %s", url, reply_str);
        goto cleanup;
    }

    json_t *message = json_object_get(error, "message");
    if (!message || strcmp(json_string_value(message), "ok") != 0) {
        log_fatal("reply error: %s: %s", url, reply_str);
        goto cleanup;
    }

    result = json_object_get(reply, "nodes");
    json_incref(result);

cleanup:
    if (curl) curl_easy_cleanup(curl);
    if (headers) curl_slist_free_all(headers);
    sdsfree(reply_str);


    printf("\n----------reply:%s\n", json_string_value(result));
    return result;
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

    CURLcode ret = curl_easy_perform(curl);
    if (ret != CURLE_OK) {
        log_fatal("get %s fail: %s", url, curl_easy_strerror(ret));
        goto cleanup;
    }

    reply = json_loads(reply_str, 0, NULL);
    if (reply == NULL) {
        log_fatal("parse %s reply fail: %s", url, reply_str);
        goto cleanup;
    }

    json_t *error;
    error = json_object_get(reply, "error");
    if (!error) {
        log_fatal("reply error: %s: %s", url, reply_str);
        goto cleanup;
    }

    json_t *message = json_object_get(error, "message");
    if (!message || strcmp(json_string_value(message), "ok") != 0) {
        log_fatal("reply error: %s: %s", url, reply_str);
        goto cleanup;
    }

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
    entry->reply = http_request(req->url, 2.0);
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

int init_jobmaster_config(void)
{
//    json_t *data = http_request(settings.jobmaster_url, 2.0);
//    if (data == NULL)
//        return -__LINE__;
//    if (settings.jobmaster_cfg)
//        json_decref(settings.jobmaster_cfg);
//    settings.jobmaster_cfg = data;
//    return 0;

    json_t *message = json_array();
    json_array_set_new(message, 1, json_string("{\"jobmasters\":[\"http://172.17.0.4:5555/newblockmonitor\"]}"));
    settings.jobmaster_cfg = message;


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


