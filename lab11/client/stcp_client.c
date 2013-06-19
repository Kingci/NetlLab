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
//  �����������ṩ��ÿ���������õ�ԭ�Ͷ����ϸ��˵��, ����Щֻ��ָ���Ե�, ����ȫ���Ը����Լ����뷨����ƴ���.
//
//  ע��: ��ʵ����Щ����ʱ, ����Ҫ����FSM�����п��ܵ�״̬, �����ʹ��switch�����ʵ��.
//
//  Ŀ��: ������������Ʋ�ʵ������ĺ���ԭ��.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// ���������ʼ��TCB��, ��������Ŀ���ΪNULL.  
// ��������ص�����TCP�׽���������conn��ʼ��һ��STCP���ȫ�ֱ���, �ñ�����Ϊsip_sendseg��sip_recvseg���������.
// ���, �����������seghandler�߳�����������STCP��. �ͻ���ֻ��һ��seghandler.
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

// ����������ҿͻ���TCB�����ҵ���һ��NULL��Ŀ, Ȼ��ʹ��malloc()Ϊ����Ŀ����һ���µ�TCB��Ŀ.
// ��TCB�е������ֶζ�����ʼ��. ����, TCB state������ΪCLOSED���ͻ��˶˿ڱ�����Ϊ�������ò���client_port. 
// TCB������Ŀ��������Ӧ��Ϊ�ͻ��˵����׽���ID�������������, �����ڱ�ʶ�ͻ��˵�����. 
// ���TCB����û����Ŀ����, �����������-1.
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

// ��������������ӷ�����. �����׽���ID�ͷ������Ķ˿ں���Ϊ�������. �׽���ID�����ҵ�TCB��Ŀ.  
// �����������TCB�ķ������˿ں�,  Ȼ��ʹ��sip_sendseg()����һ��SYN�θ�������.  
// �ڷ�����SYN��֮��, һ����ʱ��������. �����SYNSEG_TIMEOUTʱ��֮��û���յ�SYNACK, SYN �ν����ش�. 
// ����յ���, �ͷ���1. ����, ����ش�SYN�Ĵ�������SYN_MAX_RETRY, �ͽ�stateת����CLOSED, ������-1.
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

// �������ݸ�STCP������. �������ʹ���׽���ID�ҵ�TCB���е���Ŀ. 
// Ȼ����ʹ���ṩ�����ݴ���segBuf, �������ӵ����ͻ�����������. 
// ������ͻ������ڲ�������֮ǰΪ��, һ����Ϊsendbuf_timer���߳̾ͻ�����. 
// ÿ��SENDBUF_ROLLING_INTERVALʱ���ѯ���ͻ������Լ���Ƿ��г�ʱ�¼�����.
// ��������ڳɹ�ʱ����1�����򷵻�-1. 
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

// ����������ڶϿ���������������. �����׽���ID��Ϊ�������. �׽���ID�����ҵ�TCB���е���Ŀ.  
// �����������FIN�θ�������. �ڷ���FIN֮��, state��ת����FINWAIT, ������һ����ʱ��.
// ��������ճ�ʱ֮ǰstateת����CLOSED, �����FINACK�ѱ��ɹ�����. ����, ����ھ���FIN_MAX_RETRY�γ���֮��,
// state��ȻΪFINWAIT, state��ת����CLOSED, ������-1.
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

// �����������free()�ͷ�TCB��Ŀ. ��������Ŀ���ΪNULL, �ɹ�ʱ(��λ����ȷ��״̬)����1,
// ʧ��ʱ(��λ�ڴ����״̬)����-1.
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
			//�����ݷ���
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

// ������stcp_client_init()�������߳�. �������������Է������Ľ����. 
// seghandler�����Ϊһ������sip_recvseg()������ѭ��. ���sip_recvseg()ʧ��, ��˵���ص����������ѹر�,
// �߳̽���ֹ. ����STCP�ε���ʱ����������״̬, ���Բ�ȡ��ͬ�Ķ���. ��鿴�ͻ���FSM���˽����ϸ��.
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

// ����̳߳�����ѯ���ͻ������Դ�����ʱ�¼�. ������ͻ������ǿ�, ��Ӧһֱ����.
// ���(��ǰʱ�� - ��һ���ѷ��͵�δ��ȷ�϶εķ���ʱ��) > DATA_TIMEOUT, �ͷ���һ�γ�ʱ�¼�.
// ����ʱ�¼�����ʱ, ���·��������ѷ��͵�δ��ȷ�϶�. �����ͻ�����Ϊ��ʱ, ����߳̽���ֹ.
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
			//��ʱ
			if(time_tt - client->sendBufHead->sentTime > DATA_TIMEOUT){
				client->unAck_segNum = 0;
				//�ط�
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
