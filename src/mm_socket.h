#ifndef MM_SOCKET_H_
#define MM_SOCKET_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

int mm_socket(int, int, int);
int mm_socket_set_nonblock(int, int);
int mm_socket_set_nodelay(int, int);
int mm_socket_set_keepalive(int, int, int);
int mm_socket_set_nosigpipe(int, int);
int mm_socket_set_reuseaddr(int, int);
int mm_socket_error(int);
int mm_socket_connect(int, struct sockaddr*);
int mm_socket_bind(int, struct sockaddr*);
int mm_socket_listen(int, int);
int mm_socket_accept(int, struct sockaddr*, socklen_t*);
int mm_socket_write(int, void*, int);
int mm_socket_read(int, void*, int);

#endif
