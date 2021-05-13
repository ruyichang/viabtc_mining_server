# ifndef _BP_REQUEST_H_
# define _BP_REQUEST_H_

typedef int (*request_callback)(json_t *reply);

int init_request(void);
int init_jobmaster_config(void);
int update_jobmaster_config(request_callback callback);

json_t * get_peer_list(const char *url);
# endif

