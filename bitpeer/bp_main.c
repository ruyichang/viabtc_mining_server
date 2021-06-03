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

static void test_on_cron_check(nw_timer *timer, void *data)
{
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    log_error("------test_on_cron_check----sendto---begin--");

    char* buf = "testtesttest";
    auto buf_size = sizeof (buf);

    for (size_t i = 0; i < settings.jobmaster->count; ++i) {
        struct sockaddr_in *addr = &settings.jobmaster->arr[i];
        int ret = sendto(sockfd, buf, buf_size, 0, (struct sockaddr *) addr, sizeof(*addr));
        if (ret < 0) {
            char errmsg[100];
            snprintf(errmsg, sizeof(errmsg), "sendto error: %s", strerror(errno));
            log_error("errmsg:%s", errmsg);
        }
        log_error("------test_on_cron_check ret---------:%d", ret);
    }

    log_error("------test_on_cron_check----sendto---end--");
}


static void on_cron_check(nw_timer *timer, void *data)
{
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

static int init_process(void)
{
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

static int init_log(void)
{
    default_dlog = dlog_init(settings.log.path, DLOG_SHIFT_BY_DAY, 100*1024*1024, 10, 7);
    if (default_dlog == NULL)
        return -__LINE__;
    default_dlog_flag = dlog_read_flag(settings.log.flag);
    if (alert_init(&settings.alert) < 0)
        return -__LINE__;
    dlog_on_fatal = alert_msg;

    return 0;
}

int main(int argc, char *argv[])
{
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

    nw_timer_set(&test_cron_timer, 3, true, test_on_cron_check, NULL);
    nw_timer_start(&test_cron_timer);


    log_vip("server start");
    dlog_stderr("server start");
    nw_loop_run();
    log_vip("server stop");

    return 0;
}

