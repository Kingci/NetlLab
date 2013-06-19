#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include "stcp_client.h"

int connection;
int id=0;
client_tcb_t *client[MAX_TRANSPORT_CONNECTIONS];

//
//  我们在下面提供了每个函数调用的原型定义和细节说明, 但这些只是指导性的, 你完全可以根据自己的想法来设计代码.
//
//  注意: 当实现这些函数时, 你需要考虑FSM中所有可能的状态, 这可以使用switch语句来实现.
//
//  目标: 你的任务就是设计并实现下面的函数原型.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// 这个函数初始化TCB表, 将所有条目标记为NULL.  
// 它还针对重叠网络TCP套接字描述符conn初始化一个STCP层的全局变量, 该变量作为sip_sendseg和sip_recvseg的输入参数.
// 最后, 这个函数启动seghandler线程来处理进入的STCP段. 客户端只有一个seghandler.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

void stcp_client_init(int conn)
{
	int pos = 0;
	while(pos < MAX_TRANSPORT_CONNECTIONS){
		client[pos]=NULL;
		pos ++;
	}
	
	connection=conn;
	pthread_t thread;
	if (pthread_create(&thread,NULL,seghandler,&connection))
		printf("create thread for seghandler error!\n");
	return;
}

// 这个函数查找客户端TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化. 例如, TCB state被设置为CLOSED，客户端端口被设置为函数调用参数client_port. 
// TCB表中条目的索引号应作为客户端的新套接字ID被这个函数返回, 它用于标识客户端的连接. 
// 如果TCB表中没有条目可用, 这个函数返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int find(int find, int type){
	int pos = 0;
	if(find == -1){
		while(pos < MAX_TRANSPORT_CONNECTIONS){
			if(client[pos]==NULL)
				break;
			pos ++;
		}
	}
	else if(type == 1){
		while(pos < MAX_TRANSPORT_CONNECTIONS){
			if(client[pos]!=NULL){
				if(client[pos]->client_nodeID == find)
					break;
			}
			pos ++;
		}
	}
	else{
		while(pos < MAX_TRANSPORT_CONNECTIONS){
			if(client[pos]->client_nodeID==find)
				break;
			pos++;
		}
	}	
	return pos;
}

void init(int index, unsigned int client_port){
	client[index]->state=CLOSED;
	client[index]->client_portNum=client_port;
	client[index]->client_nodeID=id++;
	client[index]->next_seqNum=1000;
	client[index]->sendBufHead =NULL;
	client[index]->sendBufunSent = NULL;
	client[index]->sendBufTail = NULL;
	client[index]->bufMutex = malloc(sizeof(pthread_mutex_t));
	client[index]->unAck_segNum=0;
	pthread_mutex_init(client[index]->bufMutex, NULL);
}

int stcp_client_sock(unsigned int client_port)
{
	int index = find(-1,0);
	if(index == MAX_TRANSPORT_CONNECTIONS)
		return -1;
		
	client[index]=(client_tcb_t *)malloc(sizeof(client_tcb_t));
	init(index, client_port);
	return client[index]->client_nodeID;
}

void makeup(char *news, int type, int pos){
	if(type == 0)
		((seg_t*)news)->header.type = SYN;
	else if(type == 1)
		((seg_t*)news)->header.type = FIN;
	else
		((seg_t*)news)->header.type = DATA;
	((seg_t*)news)->header.src_port=client[pos]->client_portNum;
	((seg_t*)news)->header.dest_port=client[pos]->server_portNum;
	((seg_t*)news)->header.seq_num=client[pos]->next_seqNum;
	((seg_t*)news)->header.ack_num=0;
	((seg_t*)news)->header.length=0;
	((seg_t*)news)->header.rcv_win = 0;
	((seg_t*)news)->header.checksum = 0;
}

// 这个函数用于连接服务器. 它以套接字ID和服务器的端口号作为输入参数. 套接字ID用于找到TCB条目.  
// 这个函数设置TCB的服务器端口号,  然后使用sip_sendseg()发送一个SYN段给服务器.  
// 在发送了SYN段之后, 一个定时器被启动. 如果在SYNSEG_TIMEOUT时间之内没有收到SYNACK, SYN 段将被重传. 
// 如果收到了, 就返回1. 否则, 如果重传SYN的次数大于SYN_MAX_RETRY, 就将state转换到CLOSED, 并返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int stcp_client_connect(int sockfd, unsigned int server_port)
{
	int pos = find(sockfd,0);
	int times = 0;
	stcp_hdr_t new;
	char *news = (char *)&new;
	
	client[pos]->server_portNum = server_port;
	makeup(news, 0, pos);
	
	int x=sip_sendseg(connection, (seg_t*)news);
	client[pos]->state = SYNSENT;
	sleep(1);
	
	while (client[pos]->state == SYNSENT)	{
		times ++;
		printf("Client(state: SYNSENT): ****** Send a SYN ******\nsrc_port: %d, dest_port: %d\n", 
			((seg_t*)news)->header.src_port, ((seg_t*)news)->header.dest_port);
		sip_sendseg(connection, (seg_t*)news);
		if (times > SYN_MAX_RETRY){
			client[pos]->state = CLOSED;
			return -1;
		}
		sleep(1);	
	}
	client[pos]->next_seqNum++;
	return 1;
}

// 发送数据给STCP服务器. 这个函数使用套接字ID找到TCB表中的条目. 
// 然后它使用提供的数据创建segBuf, 将它附加到发送缓冲区链表中. 
// 如果发送缓冲区在插入数据之前为空, 一个名为sendbuf_timer的线程就会启动. 
// 每隔SENDBUF_ROLLING_INTERVAL时间查询发送缓冲区以检查是否有超时事件发生.
// 这个函数在成功时返回1，否则返回-1. 
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

void initialize(segBuf_t *segbuf, char *news, int pos){
	pthread_t thread;
	pthread_create( &thread, NULL, sendBuf_timer,client[pos]);
	client[pos]->sendBufHead = segbuf;
	client[pos]->sendBufTail = segbuf;
	segbuf->next=NULL;
	time((time_t *)&(segbuf->sentTime));
	sip_sendseg(connection, (seg_t*)news);
	client[pos]->unAck_segNum++;
}

void sendSeg(seg_t new, char *news, int pos){
	segBuf_t *segbuf = (segBuf_t*)malloc(sizeof(segBuf_t));
	segbuf->seg = new;
	if(client[pos]->sendBufHead == NULL){
		initialize(segbuf, news, pos);
	}
	else{
		client[pos]->sendBufTail->next = segbuf;
		segbuf->next = NULL;
		client[pos]->sendBufTail = client[pos]->sendBufTail->next;
		if(client[pos]->unAck_segNum < GBN_WINDOW){
			time((time_t *)&(segbuf->sentTime));
			sip_sendseg(connection, (seg_t*)news);
			client[pos]->unAck_segNum ++;
		}
		else{
			if(client[pos]->sendBufunSent = NULL){
				client[pos]->sendBufunSent=segbuf;
			}
		}
	}
}
int stcp_client_send(int sockfd, void* data, unsigned int length)
{
	int pos = find(sockfd, 0);
	int j;
	int index = 0;
	seg_t new;
	char *news = (char *)&new;
	
	pthread_mutex_lock(client[pos]->bufMutex);
	makeup(news, 3, pos);
	
	while(length > MAX_SEG_LEN){
		((seg_t*)news)->header.length = MAX_SEG_LEN;
		length = length - MAX_SEG_LEN;
		j = 0;
		while(j < MAX_SEG_LEN){
			((seg_t*)news)->data[j] = *(char*)(data);
			data ++;
			j ++;
		}

		sendSeg(new, news, pos);
		client[pos]->next_seqNum ++;
		((seg_t*)news)->header.seq_num = client[pos]->next_seqNum;
	}
	if(length != 0){
		((seg_t*)news)->header.length = length;
		j = 0;
		while(j < length){
			((seg_t*)news)->data[j] = *(char*)(data);
			data ++;
			j ++;
		}

		sendSeg(new, news, pos);
		
		client[pos]->next_seqNum ++;
	}
	pthread_mutex_unlock(client[pos]->bufMutex);
	return 1;
}

// 这个函数用于断开到服务器的连接. 它以套接字ID作为输入参数. 套接字ID用于找到TCB表中的条目.  
// 这个函数发送FIN段给服务器. 在发送FIN之后, state将转换到FINWAIT, 并启动一个定时器.
// 如果在最终超时之前state转换到CLOSED, 则表明FINACK已被成功接收. 否则, 如果在经过FIN_MAX_RETRY次尝试之后,
// state仍然为FINWAIT, state将转换到CLOSED, 并返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int stcp_client_disconnect(int sockfd)
{
	int pos=find(sockfd,1);
	int index=0;
	char *news=(char *)malloc(24);
	
	if(pos == MAX_TRANSPORT_CONNECTIONS)
		return -1;
	makeup(news, 1, pos);

	sip_sendseg(connection, (seg_t*)news);
	
	client[pos]->state = FINWAIT;
	usleep(200);	
	while (client[pos]->state == FINWAIT)	{
		index ++;
		printf("Client(state: FINWAIT): ****** Send a FIN ******\nsrc_port: %d, dest_port: %d\n",
			client[pos]->client_portNum, client[pos]->server_portNum);
		sip_sendseg(connection, (seg_t*)news);
		if (index > SYN_MAX_RETRY){
			client[pos]->state = CLOSED;
			return -1;
		}
		sleep(1);	
	}
	return 1;
}

// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int stcp_client_close(int sockfd)
{
  	free(client[sockfd]);
	client[sockfd] = NULL;
	return 1;
}

void setNULL(int pos){
	client[pos]->sendBufHead = NULL;
	client[pos]->sendBufunSent = NULL;
	client[pos]->sendBufTail = NULL;
	client[pos]->unAck_segNum = 0;
}

void FSM(seg_t seg, int pos){
	int tp=seg.header.type;
	switch(tp){
		case SYNACK:
			client[pos]->state = CONNECTED;
			break;
		case FINACK:
			client[pos]->state =CLOSED;
			break;
		case DATAACK:
			pthread_mutex_lock(client[pos]->bufMutex);
			if(client[pos]->sendBufHead == NULL){
				break;
			}
			//有数据发送
			while(client[pos]->sendBufHead != NULL){
				if(seg.header.ack_num < (client[pos]->sendBufHead->seg.header.seq_num + 1)){
					break;
				}
				if(client[pos]->sendBufHead->next != NULL){
					segBuf_t *segbuf = client[pos]->sendBufHead;
					client[pos]->sendBufHead = client[pos]->sendBufHead->next;
					client[pos]->unAck_segNum --;
					while(client[pos]->unAck_segNum < GBN_WINDOW){
						if(client[pos]->sendBufunSent != NULL){
							segBuf_t * h=client[pos]->sendBufunSent;
							client[pos]->sendBufunSent = client[pos]->sendBufunSent->next;
							time((time_t *)&h->sentTime);
							sip_sendseg(connection, &(h->seg));
							client[pos]->unAck_segNum ++;
						}
						else
							break;
					}
				}
				else{
					segBuf_t *segbuf = client[pos]->sendBufHead;
					setNULL(pos);
				}
			}
			pthread_mutex_unlock(client[pos]->bufMutex);
			break;
	}
}

// 这是由stcp_client_init()启动的线程. 它处理所有来自服务器的进入段. 
// seghandler被设计为一个调用sip_recvseg()的无穷循环. 如果sip_recvseg()失败, 则说明重叠网络连接已关闭,
// 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作. 请查看客户端FSM以了解更多细节.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
void *seghandler(void* arg)
{
  	int conn=*(int*)arg;
	seg_t seg;
	while(1){
		int pos = 0;
		if(sip_recvseg(connection, &seg) == -1)
			break;
		while(pos < MAX_TRANSPORT_CONNECTIONS){
			if(client[pos] != NULL){
				if(client[pos]->server_portNum == seg.header.src_port && 
					client[pos]->client_portNum == seg.header.dest_port){
					break;
				}
			}
			pos++;
		}
		if(pos == MAX_TRANSPORT_CONNECTIONS){
			continue;
		}
		FSM(seg, pos);
	}
}

// 这个线程持续轮询发送缓冲区以触发超时事件. 如果发送缓冲区非空, 它应一直运行.
// 如果(当前时间 - 第一个已发送但未被确认段的发送时间) > DATA_TIMEOUT, 就发生一次超时事件.
// 当超时事件发生时, 重新发送所有已发送但未被确认段. 当发送缓冲区为空时, 这个线程将终止.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
void* sendBuf_timer(void* clienttcb)
{	
	client_tcb_t *client=(client_tcb_t *)clienttcb;
	while(1){
		usleep(100);
		if(client->sendBufHead != NULL)
		{
			long int time_tt;
			time((time_t *)&time_tt);
			pthread_mutex_lock(client->bufMutex);
			//超时
			if(time_tt - client->sendBufHead->sentTime > DATA_TIMEOUT){
				client->unAck_segNum = 0;
				//重发
				segBuf_t * head = client->sendBufHead;
				while(head != NULL){
					if(client->unAck_segNum > GBN_WINDOW)
						break;
					if(head == client->sendBufunSent)
						client->sendBufunSent = client->sendBufunSent->next;
					time((time_t *)&head->sentTime);
					sip_sendseg(connection, &(head->seg));
					client->unAck_segNum ++;
					head = head->next;
				}
			}
			pthread_mutex_unlock(client->bufMutex);
		}
		else
			pthread_exit(NULL);
	}
}
