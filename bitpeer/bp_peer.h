/*
 * Description: 
 *     History: yang@haipo.me, 2016/06/27, create
 */

# ifndef _BP_PEER_H_
# define _BP_PEER_H_

# define HEX_ZERO "0000000000000000000000000000000000000000000000000000000000000000"
# define MAGIC_NUMBER 0xF6666666u
# define UDP_TIMES 3

int init_peer(void);
int update_peer(void);
int broadcast_block(void *block, size_t block_size);
int broadcast_header(void *block, size_t block_size);
sds get_peer_status(void);
int get_peer_num(void);
int get_peer_limit(void);

# endif

