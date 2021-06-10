/*
 * Description: 
 *     History: yang@haipo.me, 2016/06/27, create
 */

# include "ut_title.h"
# include "ut_signal.h"
# include "bp_config.h"
# include "bp_server.h"
# include "bp_peer.h"
# include "bp_cli.h"
# include "bp_request.h"

const char *version = "0.1.0";
nw_timer cron_timer;
nw_timer test_cron_timer;

static int sockfd;

static void test_on_cron_check(nw_timer *timer, void *data) {

    uint32_t magic_ = htole32(MAGIC_NUMBER);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    log_error("------test_on_cron_check----sendto---begin--");


    json_t *message = json_object();
    json_object_set_new(message, "height", json_integer(123123));
    json_object_set_new(message, "curtime", json_integer(1622787234));
    json_object_set_new(message, "hash",json_string("0000000000000000000000000000000000000000000000000000000000000000"));
    json_object_set_new(message, "prevhash",json_string("0000000000000000000000000000000000000000000000000000000000000000"));
    json_object_set_new(message, "bits", json_string("0xF7777777u"));


    char *message_data = json_dumps(message, 0);
    if (message_data == NULL) {
        log_error("json_dumps fail");
        json_decref(message);
        return -__LINE__;
    }
    log_debug("block json msg: %s", message_data);

    auto buf_size = strlen(message_data);
    log_error("------test_on_cron_check--1--");

    char *msg_send_buf = malloc(4+2 + buf_size + 1); //magic + len +data
    log_debug("======sizeof (magic_):%d, buf_size:%d", sizeof(magic_), buf_size);

    memset(msg_send_buf, 0, 4 + 2 + buf_size +1);
//    int ret = snprintf(msg_send_buf, 4+2 +buf_size +1, "%x%x%s\n", magic_, buf_size, message_data);


    size_t left = 4 + 2 + buf_size +1;
    pack_uint32_le(&msg_send_buf, &left, MAGIC_NUMBER);
    pack_uint32_le(&msg_send_buf, &left, buf_size);
    pack_varstr(&msg_send_buf, &left, message, buf_size);
    log_debug("@@@@@@@@@@@@@@block notify msg: %s", msg_send_buf);

    for (size_t i = 0; i < settings.jobmaster->count; ++i) {
        struct sockaddr_in *addr = &settings.jobmaster->arr[i];

        char str[128];
        char ip[46];
        inet_ntop(2, &addr->sin_addr, ip, sizeof(ip));
        snprintf(str, sizeof(str), "%s:%u", ip, ntohs(addr->sin_port));
        log_error("--send to--:%s", str);

        int ret = sendto(sockfd, msg_send_buf, 4+2 +buf_size, 0, (struct sockaddr *) addr, sizeof(*addr));
        if (ret < 0) {
            char errmsg[100];
            snprintf(errmsg, sizeof(errmsg), "sendto error: %s", strerror(errno));
            log_error("errmsg:%s", errmsg);
        }
        log_error("------test_on_cron_check ret---------:%d", ret);
        close(sockfd);
    }
}


static void on_cron_check(nw_timer *timer, void *data) {
    dlog_check_all();
    if (signal_exit) {
        nw_loop_break();
        signal_exit = 0;
    }
    if (signal_reload) {
        log_info("start update peer");
        int ret = update_peer();
        if (ret < 0) {
            log_error("update peer fail: %d", ret);
        } else {
            log_error("update peer success");
        }
        signal_reload = 0;
    }
}

static int init_process(void) {
    if (settings.process.file_limit) {
        if (set_file_limit(settings.process.file_limit) < 0) {
            return -__LINE__;
        }
    }
    if (settings.process.core_limit) {
        if (set_core_limit(settings.process.core_limit) < 0) {
            return -__LINE__;
        }
    }

    return 0;
}

static int init_log(void) {
    default_dlog = dlog_init(settings.log.path, DLOG_SHIFT_BY_DAY, 100 * 1024 * 1024, 10, 7);
    if (default_dlog == NULL)
        return -__LINE__;
    default_dlog_flag = dlog_read_flag(settings.log.flag);
    if (alert_init(&settings.alert) < 0)
        return -__LINE__;
    dlog_on_fatal = alert_msg;

    return 0;
}

int main(int argc, char *argv[]) {
    printf("process: %s version: %s, compile date: %s %s\n", "bitpeer", version, __DATE__, __TIME__);

    if (argc != 2) {
        printf("usage: %s config.json\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    if (process_exist("bitpeer.exe") != 0) {
        printf("process exist\n");
        exit(EXIT_FAILURE);
    }

    process_title_init(argc, argv);
    process_title_set("bitpeer.exe");

    int ret;
    ret = load_config(argv[1]);
    if (ret < 0) {
        error(EXIT_FAILURE, errno, "load config fail: %d", ret);
    }
    ret = init_process();
    if (ret < 0) {
        error(EXIT_FAILURE, errno, "init process fail: %d", ret);
    }
    ret = init_log();
    if (ret < 0) {
        error(EXIT_FAILURE, errno, "init log fail: %d", ret);
    }

    daemon(1, 1);
    process_keepalive();

    ret = init_server();
    if (ret < 0) {
        error(EXIT_FAILURE, errno, "init server fail: %d", ret);
    }
    ret = init_request();
    if (ret < 0) {
        error(EXIT_FAILURE, errno, "init request fail: %d", ret);
    }
    ret = init_cli();
    if (ret < 0) {
        error(EXIT_FAILURE, errno, "init cli fail: %d", ret);
    }
    ret = init_peer();
    if (ret < 0) {
        error(EXIT_FAILURE, errno, "init peer fail: %d", ret);
    }

    nw_timer_set(&cron_timer, 0.1, true, on_cron_check, NULL);
    nw_timer_start(&cron_timer);

    nw_timer_set(&test_cron_timer, 30, true, test_on_cron_check, NULL);
    nw_timer_start(&test_cron_timer);


    log_vip("server start");
    dlog_stderr("server start");
    nw_loop_run();
    log_vip("server stop");

    return 0;
}

