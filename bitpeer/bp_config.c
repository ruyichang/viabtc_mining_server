/*
 * Description: 
 *     History: yang@haipo.me, 2016/06/27, create
 */

# include "bp_config.h"
# include "bp_request.h"

struct settings settings;

static void inetv4_list_free(inetv4_list *list) {
    if (list) {
        if (list->arr)
            free(list->arr);
        free(list);
    }
}

static int load_cfg_jobmaster(json_t *root, const char *key) {
    int ret = read_cfg_str(root, key, &settings.jobmaster_url, NULL);
    if (ret < 0) {
        printf("[load_cfg_jobmaster]load cfg jobmaster_url fail.\n");
        return -__LINE__;
    }

    ret = init_jobmaster_config();
    if (ret < 0) {
        return -__LINE__;
    }

    settings.jobmaster = malloc(sizeof(inetv4_list));
    ret = load_cfg_inetv4_list_direct(settings.jobmaster_cfg, settings.jobmaster);
    if (ret < 0) {
        char *str = json_dumps(settings.jobmaster_cfg, 0);
        log_error("load cfg jobmaster fail, jobmaster_cfg: %s", str);
        free(str);
        return -__LINE__;
    }

    // test logs
    char *str_ = json_dumps(settings.jobmaster_cfg, 0);
    printf("[load_cfg_jobmaster]load cfg jobmaster fail, jobmaster_cfg: %s", str_);
    free(str_);

    return 0;
}

static int load_cfg_friend_pools(json_t *root, const char *key) {
    int ret = read_cfg_str(root, key, &settings.friend_pools_url, NULL);
    if (ret < 0) {
        printf("[load_cfg_friend_pools]load cfg friend_pools_url fail.\n");
        return -__LINE__;
    }

    ret = init_friend_pools_config();
    if (ret < 0) {
        return -__LINE__;
    }

    return 0;
}

int do_load_config(json_t *root) {
    int ret;
    ret = load_cfg_process(root, "process", &settings.process);
    if (ret < 0) {
        printf("load process config fail: %d\n", ret);
        return -__LINE__;
    }
    ret = load_cfg_log(root, "log", &settings.log);
    if (ret < 0) {
        printf("load log config fail: %d\n", ret);
        return -__LINE__;
    }
    ret = load_cfg_alert(root, "alert", &settings.alert);
    if (ret < 0) {
        printf("load alert config fail: %d\n", ret);
        return -__LINE__;
    }
    ret = load_cfg_svr(root, "svr", &settings.svr);
    if (ret < 0) {
        printf("oad svr config fail: %d\n", ret);
        return -__LINE__;
    }
    ret = load_cfg_cli_svr(root, "cli", &settings.cli);
    if (ret < 0) {
        printf("load cli config fail: %d\n", ret);
        return -__LINE__;
    }
    ret = load_cfg_coin_rpc(root, "coin", &settings.coin);
    if (ret < 0) {
        printf("load coin config fail: %d\n", ret);
        return -__LINE__;
    }
    ret = read_cfg_str(root, "request_auth", &settings.request_auth, NULL);
    if (ret < 0) {
        printf("read request_auth fail: %d\n", ret);
        return -__LINE__;
    }

    ret = load_cfg_jobmaster(root, "jobmaster_url");
    if (ret < 0) {
        printf("load cfg jobmaster fail: %d\n", ret);
        return -__LINE__;
    }

    ret = load_cfg_friend_pools(root, "friend_pools_url");
    if (ret < 0) {
        printf("load cfg friend_pools fail: %d\n", ret);
        return -__LINE__;
    }

    ret = read_cfg_int(root, "jobmaster_update_interval", &settings.jobmaster_update_interval, false, 30);
    if (ret < 0) {
        printf("read jobmaster_update_interval fail: %d\n", ret);
        return -__LINE__;
    }
    ret = read_cfg_str(root, "peer_config_path", &settings.peer_config_path, NULL);
    if (ret < 0) {
        printf("read peer_config_path fail: %d\n", ret);
        return -__LINE__;
    }
    ret = read_cfg_int(root, "reconnect_timeout", &settings.reconnect_timeout, false, 60);
    if (ret < 0) {
        printf("read reconnect_timeout fail: %d\n", ret);
        return -__LINE__;
    }
    ret = read_cfg_int(root, "broadcast_limit", &settings.broadcast_limit, false, 60);
    if (ret < 0) {
        printf("read broadcast_limit fail: %d\n", ret);
        return -__LINE__;
    }

    //for peer list get
    ret = read_cfg_str(root, "peer_country", &settings.peer_country, NULL);
    if (ret < 0) {
        printf("read peer_country fail: %d\n", ret);
        return -__LINE__;
    }
    ret = read_cfg_str(root, "peer_city", &settings.peer_city, NULL);
    if (ret < 0) {
        printf("read peer_city fail: %d\n", ret);
        return -__LINE__;
    }
    ret = read_cfg_str(root, "peer_list_url", &settings.peer_list_url, NULL);
    if (ret < 0) {
        printf("read peer_list_url fail: %d\n", ret);
        return -__LINE__;
    }

    char *start_string = NULL;
    ret = read_cfg_str(root, "start_string", &start_string, NULL);
    if (ret < 0) {
        printf("read start_string fail: %d\n", ret);
        return -__LINE__;
    }
    settings.start_string = hex2bin(start_string);
    if (settings.start_string == NULL || sdslen(settings.start_string) != 4) {
        printf("read start_string fail: %d\n", ret);
        return -__LINE__;
    }
    ret = load_cfg_http_svr(root, "http_svr", &settings.http_svr);
    if (ret < 0) {
        printf("load http svr config fail: %d\n", ret);
        return -__LINE__;
    }

    return 0;
}

int load_config(const char *path) {
    json_error_t error;
    json_t *root = json_load_file(path, 0, &error);
    if (root == NULL) {
        printf("json_load_file from: %s fail: %s in line: %d\n", path, error.text, error.line);
        return -__LINE__;
    }
    if (!json_is_object(root)) {
        json_decref(root);
        return -__LINE__;
    }
    int ret = do_load_config(root);
    if (ret < 0) {
        json_decref(root);
        return ret;
    }
    json_decref(root);
    printf("load_config done!\n");

    return 0;
}

