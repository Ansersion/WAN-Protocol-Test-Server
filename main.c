#include <errno.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <signal.h>

#include "wanp.h"
#include "checksum.h"
#include "cmd.h"

#define WAN_MSG_MAX_SIZE 	65536
#define TIME_OUT 			30 	// in unit of second
#define WAN_PORT 			33027
#define LISTENQ 			1

#define MAX_ARGC 			10
#define ARGV_BUF_SIZE 		64

extern const unsigned char * WAN_EndFlag;
extern const int WAN_EndFlagSize;

extern const Cmd Hello;
extern const Cmd Burn;
extern const Cmd Startos;

char argv[MAX_ARGC][ARGV_BUF_SIZE];
int argc;
// int GetArgcArgv(int * argc, char ** argv);
// void InitArgv();

uint8_t kernel_name[64];
uint8_t kernel_size[32];
uint8_t addr[32];
uint8_t crc[32];

uint32_t i_addr;
uint32_t i_kernel_size;
uint32_t i_crc;

int	socket_alive;
void sig_pipe_handler(int sig);
void sig_pipe_handler(int sig)
{
	socket_alive = 0;
}

void main()
{
	int 				ret;
	int					i, j, k, maxfd, listenfd, connfd;
	// int 				sockfd[LISTENQ];
	int 				reuse = 1;
	fd_set				rset, wset;
	char				buf[WAN_MSG_MAX_SIZE];
	char 				cmd_buf[WAN_MSG_MAX_SIZE];
	char 				tmp_buf[WAN_MSG_MAX_SIZE];
	char * 				tmp_buf_pointer;
	char * 				endptr;
	char  				opt[64];
	socklen_t			clilen;
	struct sockaddr_in	cliaddr, servaddr;
	uint16_t 			csum;
	uint16_t 			msg_size_net;
	uint16_t 			msg_size;

	signal(SIGPIPE, sig_pipe_handler);
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if(listenfd < 0) {
		perror("socket");
		exit(1);
	}
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
		perror("setsockopet");
		exit(1);
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons(WAN_PORT);

	if(bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		perror("bind");
		exit(1);
	}
	listen(listenfd, LISTENQ);
	maxfd = listenfd;			/* initialize */
	do {
		clilen = sizeof(cliaddr);
		connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);

		socket_alive = 1;
		while(socket_alive) {
			FD_ZERO(&rset);
			FD_SET(connfd, &rset);
			maxfd = connfd;
			switch(select(maxfd+1, &rset, NULL, NULL, NULL)) {
				case -1: 
					socket_alive = 0;
					close(connfd);
					break;
				case 0:
					break;
				default:
#ifdef WANP
					if(recv(connfd, buf, WAN_HEADER_SIZE, MSG_WAITALL) != WAN_HEADER_SIZE) {
						perror("recv");
						close(connfd);
						break;
					}
					Wan_GetSize(buf, &msg_size);
					if(recv(connfd, buf + WAN_HEADER_SIZE, msg_size - WAN_HEADER_SIZE, MSG_WAITALL) != msg_size - WAN_HEADER_SIZE) {
						perror("recv");
						close(connfd);
						break;
					}
					if(Wan_CheckMsg(buf, msg_size) < 0) {
						perror("wan message check error");
						close(connfd);
						break;
					}
#else
					for(i = 0; i < WAN_MSG_MAX_SIZE; i++) {
						if(recv(connfd, buf+i, 1, 0) != 1) {
							perror("recv");
							close(connfd);
							break;
						}
						if('\n' == buf[i]) {
							msg_size = i + 1;
							break;
						}
					}
#endif
					buf[msg_size] = '\0';
					GetCmd(buf, cmd_buf);
					// printf("cmd: %s\n", cmd_buf);
					// printf("cmd(buf): %s\n", buf);
					*tmp_buf = '\0';
					if(strcmp(cmd_buf, Hello.Name) == 0) {
						ret = DoHello(buf);
						if(0 == ret) {
							RespOK(buf, cmd_buf, NULL);
						} else if(HELP_HELLO == ret) {
							tmp_buf_pointer = tmp_buf;
							tmp_buf_pointer += sprintf(tmp_buf_pointer, "hello: \r\n");
							tmp_buf_pointer += sprintf(tmp_buf_pointer, "	a test message to confirm connection.\r\n");
							RespOK(buf, cmd_buf, tmp_buf);
						} else {
							RespErr(buf, cmd_buf, "hello err");
						}
					} else if(strcmp(cmd_buf, Burn.Name) == 0) {
						ret = DoBurn(buf, kernel_name, addr, kernel_size, crc);
						if(ret == 0) {
							if('0' == addr[0] && ('x' == addr[1] || 'X' == addr[1])) {
								i_addr = strtol((const char *)addr, &endptr, 16);
							} else {
								i_addr = atoi((const char *)addr);
							}
							if('0' == kernel_size[0] && ('x' == kernel_size[1] || 'X' == kernel_size[1])) {
								i_kernel_size = strtol((const char *)kernel_size, &endptr, 16);
							} else {
								i_kernel_size = atoi((const char *)kernel_size);
							}
							if('0' == crc[0] && ('x' == crc[1] || 'X' == crc[1])) {
								i_crc = strtol((const char *)crc, &endptr, 16);
							} else {
								i_crc = atoi((const char *)crc);
							}
							// i_addr = atoi(addr);
							// i_kernel_size = atoi(kernel_size);
							// i_crc = atoi(crc);
							// printf("i_addr: %d\n", i_addr);
							// printf("i_kernel_size: %d\n", i_kernel_size);
							// printf("i_crc: %d\n", i_crc);
							printf("u_addr:u_kernel_size:u_crc=%08x:%08x:%08x\r\n", i_addr, i_kernel_size, i_crc);
							RespOK(buf, cmd_buf, NULL);
						} else if(HELP_BURN == ret) {
							tmp_buf_pointer = tmp_buf;
							tmp_buf_pointer += sprintf(tmp_buf_pointer, "burn: \r\n");
							tmp_buf_pointer += sprintf(tmp_buf_pointer, "	burn kernel to specific address.\r\n");
							tmp_buf_pointer += sprintf(tmp_buf_pointer, "	-f\r\n");
							tmp_buf_pointer += sprintf(tmp_buf_pointer, "	-a\r\n");
							RespOK(buf, cmd_buf, tmp_buf);
						} else {
							RespErr(buf, cmd_buf, "paramter err");
						}
					} else if(strcmp(cmd_buf, Startos.Name) == 0) {
						ret = DoStartos(buf);
						if(0 == ret) {
							RespOK(buf, cmd_buf, NULL);
						} else if(HELP_STARTOS == ret) {
							tmp_buf_pointer = tmp_buf;
							// j = sprintf(tmp_buf_pointer, "startos: \r\n");
							tmp_buf_pointer += sprintf(tmp_buf_pointer, "startos: \r\n");
							// tmp_buf_pointer += j;
							tmp_buf_pointer += sprintf(tmp_buf_pointer, "	launch the OS kernel.\r\n");
							*tmp_buf_pointer = '\0';
							RespOK(buf, cmd_buf, tmp_buf);
						} else {
							RespErr(buf, cmd_buf, "startos err");
						}
					} else {
						RespErr(buf, cmd_buf, "Command not found");
					}
					if((msg_size = SealPacket(buf)) < 0) {
						printf("SealPacket error\r\n");
						break;
					}

					printf("send: ");
					for(i = 0; i < msg_size; i++) {
						printf("%c", buf[i]);
					}
					printf("###\n");
					if(send(connfd, buf, msg_size, 0) < 0) {
						perror("send");
						close(connfd);
						break;
					}
					printf("\n");
			}
		}

	}while(1);
}

