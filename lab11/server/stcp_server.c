// 文件名: stcp_server.c
//
// 描述: 这个文件包含服务器STCP套接字接口定义. 你需要实现所有这些接口.
//
// 创建日期: 2013年1月
//

#include <stdlib.h>
#include <sys/socket.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include "stcp_server.h"
#include "../common/constants.h"

int id=0;
int connection=0;
server_tcb_t *serv[MAX_TRANSPORT_CONNECTIONS];

//
//  用于服务器程序的STCP套接字API. 
//  ===================================
//
//  我们在下面提供了每个函数调用的原型定义和细节说明, 但这些只是指导性的, 你完全可以根据自己的想法来设计代码.
//
//  注意: 当实现这些函数时, 你需要考虑FSM中所有可能的状态, 这可以使用switch语句来实现. 
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

// 这个函数初始化TCB表, 将所有条目标记为NULL. 它还针对重叠网络TCP套接字描述符conn初始化一个STCP层的全局变量, 
// 该变量作为sip_sendseg和sip_recvseg的输入参数. 最后, 这个函数启动seghandler线程来处理进入的STCP段.
// 服务器只有一个seghandler.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

void stcp_server_init(int conn)
{
	int pos = 0;
	while(pos < MAX_TRANSPORT_CONNECTIONS){
		serv[pos]=NULL;
		pos++;
	}
	connection=conn;
	pthread_t thread;
	if (pthread_create(&thread, NULL, seghandler, &conn))
		printf("create thread for seghandler error!\n");
	return;
}

// 这个函数查找服务器TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化, 例如, TCB state被设置为CLOSED, 服务器端口被设置为函数调用参数server_port. 
// TCB表中条目的索引应作为服务器的新套接字ID被这个函数返回, 它用于标识服务器的连接. 
// 如果TCB表中没有条目可用, 这个函数返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int find (int sockfd){
	int pos = 0;
	if(sockfd == -1){
		while (pos < MAX_TRANSPORT_CONNECTIONS) {
			if (serv[pos] == NULL)
				break;
			pos ++;
		}
	}
	else{
		while(pos<MAX_TRANSPORT_CONNECTIONS){
			if(serv[pos]->server_nodeID==sockfd)
				break;
			pos++;
		}
	}
	return pos;
}

void init(int index, unsigned int server_port){
	serv[index]->state=CLOSED;
	serv[index]->server_portNum=server_port;
	serv[index]->server_nodeID=id++;
	serv[index]->expect_seqNum=1000;
	serv[index]->recvBuf = malloc(RECEIVE_BUF_SIZE);
	serv[index]->bufMutex = malloc(sizeof(pthread_mutex_t));
	serv[index]->usedBufLen=0;
	pthread_mutex_init(serv[index]->bufMutex, NULL);
}

int stcp_server_sock(unsigned int server_port)
{
	int index = find(-1);

	if(index==-1)
		return -1;
	serv[index]=(server_tcb_t *)malloc(sizeof(server_tcb_t));
	init(index, server_port);
	
	printf("Server: in function stcp_server_sock, initialize!! sock is %d\n", index);
	return serv[index]->server_nodeID;
}

// 这个函数使用sockfd获得TCB指针, 并将连接的state转换为LISTENING. 它然后启动定时器进入忙等待直到TCB状态转换为CONNECTED 
// (当收到SYN时, seghandler会进行状态的转换). 该函数在一个无穷循环中等待TCB的state转换为CONNECTED,  
// 当发生了转换时, 该函数返回1. 你可以使用不同的方法来实现这种阻塞等待.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_server_accept(int sockfd)
{	
	serv[sockfd]->state = LISTENING;
	while (1){
		if(serv[sockfd]->state == CONNECTED)
			break;
	}
	return 1;
}

// 接收来自STCP客户端的数据. 请回忆STCP使用的是单向传输, 数据从客户端发送到服务器端.
// 信号/控制信息(如SYN, SYNACK等)则是双向传递. 这个函数每隔RECVBUF_ROLLING_INTERVAL时间
// 就查询接收缓冲区, 直到等待的数据到达, 它然后存储数据并返回1. 如果这个函数失败, 则返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
static int tem=0;
int stcp_server_recv(int sockfd, void* buf, unsigned int length, int sta)
{
	int pos = find(sockfd);
	printf("Server: ****** Recieve Data from client ******  \n");
	while(1){	
		pthread_mutex_lock(serv[pos]->bufMutex);
		if((length < RECEIVE_BUF_SIZE) && (tem == sta)){
			if(serv[pos]->usedBufLen >= length){
				memcpy(buf, serv[pos]->recvBuf,length);
				serv[pos]->usedBufLen -= length;
				int j = 0;
				for(; j < serv[pos]->usedBufLen; j++)
					serv[pos]->recvBuf[j] = serv[pos]->recvBuf[j + length];
				tem ++;
				pthread_mutex_unlock(serv[pos]->bufMutex);
				return 1;
			}
		}
		pthread_mutex_unlock(serv[pos]->bufMutex);
	}
}

// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int stcp_server_close(int sockfd)
{
	free(serv[sockfd]);
	serv[sockfd] = NULL;
	return 1;
}

// 这是由stcp_server_init()启动的线程. 它处理所有来自客户端的进入数据. seghandler被设计为一个调用sip_recvseg()的无穷循环, 
// 如果sip_recvseg()失败, 则说明重叠网络连接已关闭, 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作.
// 请查看服务端FSM以了解更多细节.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

void sendseg(unsigned short flag,int pos,unsigned int ack){
	char *segment=(char *)malloc(24);
	unsigned short *temp = (unsigned short*)segment;
	((stcp_hdr_t*)segment)->src_port=serv[pos]->server_portNum;
	((stcp_hdr_t*)segment)->dest_port=serv[pos]->client_portNum;
	((stcp_hdr_t*)segment)->seq_num = 0;
	((stcp_hdr_t*)segment)->ack_num = ack;
	((stcp_hdr_t*)segment)->length = 0;
	((stcp_hdr_t*)segment)->type = flag;
	((stcp_hdr_t*)segment)->rcv_win = 0;
	((stcp_hdr_t*)segment)->checksum = 0;
	
	sip_sendseg(connection, (seg_t*)segment);
}
void* seghandler(void* arg)
{
  	int conn=*(int*)arg;
	seg_t seg;
	
	while(1){
		int t = sip_recvseg(connection, &seg);
		if(t == -1)
			break;
		else if(t != 1){
			int pos = 0;
			while(pos < MAX_TRANSPORT_CONNECTIONS){
				if(serv[pos]!=NULL && serv[pos]->server_portNum == seg.header.dest_port)
					break;
				pos ++;
			}
			if(pos != MAX_TRANSPORT_CONNECTIONS){
				int seg_type = seg.header.type;
				switch(seg_type){
					case SYN:
						serv[pos]->client_portNum = seg.header.src_port;
						serv[pos]->state = CONNECTED;
						
						if(serv[pos]->expect_seqNum == seg.header.seq_num){
							printf("Server(state: CONNECTED): ****** SYN from client ****** send a SYNACK ****** \n     src_port: %d, dest_port: %d\n",
								serv[pos]->server_portNum, serv[pos]->client_portNum); 
							printf("state: LISTENING ---> state: CONNECTED\n");
							serv[pos]->expect_seqNum ++;
						}
						else if(serv[pos]->expect_seqNum >= seg.header.seq_num)
							sendseg(SYNACK,pos,seg.header.seq_num + 1);
					break;
					case FIN:
						serv[pos]->state = CLOSEWAIT;
						if(serv[pos]->expect_seqNum == seg.header.seq_num){
							printf("Server(state: CONNECTED): ****** FIN from client ****** send a FINACK ****** \n     src_port: %d, dest_port: %d\n",
								serv[pos]->server_portNum, serv[pos]->client_portNum); 
							printf("state: CONNECTED ---> state: CLOSEWAIT\n");
							serv[pos]->expect_seqNum ++;
						}
						if(serv[pos]->expect_seqNum >= seg.header.seq_num){
							sendseg(FINACK,pos,seg.header.seq_num + 1);
						}
					break;
					case DATA:
			//			printf("Server(state: CONNECTED): ****** DATA from client ****** send a DATAACK ****** \n");
						if(serv[pos]->expect_seqNum == seg.header.seq_num){
							serv[pos]->expect_seqNum ++;
							pthread_mutex_lock(serv[pos]->bufMutex);
							int j=0;
							while(j < seg.header.length){
								if(serv[pos]->usedBufLen < RECEIVE_BUF_SIZE){
									serv[pos]->recvBuf[serv[pos]->usedBufLen] = seg.data[j];
									serv[pos]->usedBufLen++;
								}
								j++;
							}	
			//				printf("serv[pos]->usedBufLen %d\n", serv[pos]->usedBufLen);
							pthread_mutex_unlock(serv[pos]->bufMutex);
						}
						if(serv[pos]->expect_seqNum >= seg.header.seq_num){
			//				printf("sendack:%d\n", seg.header.seq_num+1);
							sendseg(DATAACK,pos, seg.header.seq_num+1);
						}
					break;
				}
			}
		}
	}


  return 0;
}