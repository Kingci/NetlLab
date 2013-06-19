//文件名: son/son.c
//
//描述: 这个文件实现SON进程 
//SON进程首先连接到所有邻居, 然后启动listen_to_neighbor线程, 每个该线程持续接收来自一个邻居的进入报文, 并将该报文转发给SIP进程. 
//然后SON进程等待来自SIP进程的连接. 在与SIP进程建立连接之后, SON进程持续接收来自SIP进程的sendpkt_arg_t结构, 并将接收到的报文发送到重叠网络中. 
//
//创建日期: 2013年1月

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <sys/utsname.h>
#include <assert.h>
#include "son.h"
//你应该在这个时间段内启动所有重叠网络节点上的SON进程
#define SON_START_DELAY 15

/**************************************************************/
//声明全局变量
/**************************************************************/

//将邻居表声明为一个全局变量 
//将与SIP进程之间的TCP连接声明为一个全局变量
int sip_conn; 

/**************************************************************/
//实现重叠网络函数
/**************************************************************/

// 这个线程打开TCP端口CONNECTION_PORT, 等待节点ID比自己大的所有邻居的进入连接,
// 在所有进入连接都建立后, 这个线程终止. 

void* waitNbrs(void* arg) {
	int listenfd, connfd;
	int option = 1;
	struct sockaddr_in caddr, saddr;
	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		printf("Listen is Wrong !!!!!!!\n");
		exit(2);
	}
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) < 0){
		printf("Error!!!!!\n");
		exit(2);
	}
	
	memset(&saddr, 0, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);
	saddr.sin_port = htons(CONNECTION_PORT);
	bind(listenfd, (struct sockaddr*)&saddr, sizeof(saddr));
	listen(listenfd, 2);
	
	socklen_t len = sizeof(caddr);
	int i;
	for (i = 0; i < topology_getNbrNum(); i++){
		if(nt[i].nodeID > topology_getMyNodeID()){
			connfd = accept(listenfd, (struct sockaddr*)&caddr, &len);
			if(connfd <= 0)
				printf("Accept the Socket Wrong !!!!!!!!\n");
			printf("*****************success %d******************\n",i);
			inet_ntoa(*(struct in_addr *)&caddr.sin_addr.s_addr); 
			int t=0;
			for(t=0; t < topology_getNbrNum(); t++){
				if((nt[t]).nodeIP == caddr.sin_addr.s_addr){
					(nt[t]).conn = connfd;
					printf("waitNbrs:connect success %d\n",nt[t].nodeID);
					break;
				}
			}
		}
	}
	pthread_exit(NULL);
}

// 这个函数连接到节点ID比自己小的所有邻居.
// 在所有外出连接都建立后, 返回1, 否则返回-1.
int connectNbrs() {
	printf("neighbornum is %d\n",topology_getNbrNum());
	int i;
	for (i = 0; i < topology_getNbrNum(); i++){
		if(nt[i].nodeID < topology_getMyNodeID()){
			int sockfd = socket(AF_INET, SOCK_STREAM, 0);
			struct sockaddr_in addr;
			if (sockfd < 0){
				printf("Create Socket Wrong !!!!!!!\n");
				return -1;
			}
				
			memset(&addr, 0, sizeof(addr));
			addr.sin_family = AF_INET;
			addr.sin_port = htons(CONNECTION_PORT);
			addr.sin_addr.s_addr = nt[i].nodeIP;
			
			if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0){
				printf("Connect the Socket Wring !!!!!!!!\n");
				return -1;
			}
			nt[i].conn = sockfd;
			printf("connectNbrs:connect success %d\n", nt[i].nodeID);
		}
	}
	return 1;
}

//每个listen_to_neighbor线程持续接收来自一个邻居的报文. 它将接收到的报文转发给SIP进程.
//所有的listen_to_neighbor线程都是在到邻居的TCP连接全部建立之后启动的. 
void* listen_to_neighbor(void* arg) {
	sip_pkt_t* pkt = (sip_pkt_t *)malloc(sizeof(sip_pkt_t));
	while(1){
		memset(pkt, 0, sizeof(sip_pkt_t));
		if(recvpkt(pkt,nt[*(int *)arg].conn) > 0)
			forwardpktToSIP(pkt, sip_conn);
		else
			pthread_exit(NULL);
	}
}

//这个函数打开TCP端口SON_PORT, 等待来自本地SIP进程的进入连接. 
//在本地SIP进程连接之后, 这个函数持续接收来自SIP进程的sendpkt_arg_t结构, 并将报文发送到重叠网络中的下一跳. 
//如果下一跳的节点ID为BROADCAST_NODEID, 报文应发送到所有邻居节点.
void waitSIP() {
	int listenfd, connfd;
	int option = 1;
	struct sockaddr_in caddr, saddr;
	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		printf("Listen is Wrong !!!!!!!\n");
		exit(2);
	}
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) < 0){
		printf("Error!!!!!\n");
		exit(2);
	}

	memset(&saddr, 0, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);
	saddr.sin_port = htons(SON_PORT);
	bind(listenfd, (struct sockaddr*)&saddr, sizeof(saddr));
	listen(listenfd, 2);
	
	socklen_t len = sizeof(caddr);
	connfd = accept(listenfd, (struct sockaddr*)&caddr, &len);
	close(listenfd);
	
	sip_conn = connfd;
	sip_pkt_t* pkt = (sip_pkt_t *)malloc(sizeof(sip_pkt_t));
	int *nextNode = (int *)malloc(sizeof(int));
	while(1){
		if(getpktToSend(pkt, nextNode,sip_conn) > 0){
			if(*nextNode == BROADCAST_NODEID){
				int j = 0;
				while(j < neighNum){
					sendpkt(pkt, nt[j].conn);
					j ++;
				}
			}
			else{
				int j = 0;
				while(j < neighNum){
					if(*nextNode == nt[j].nodeID){
						sendpkt(pkt, nt[j].conn);
						j ++;
					}
				}
			}
		}
	}
}

//这个函数停止重叠网络, 当接收到信号SIGINT时, 该函数被调用.
//它关闭所有的连接, 释放所有动态分配的内存.
void son_stop() {
	int i = 0;
	while(i < neighNum){
		close(nt[i].conn);
		i ++;
	}
	free(nt);
}

int main() {
	//启动重叠网络初始化工作
	printf("Overlay network: Node %d initializing...\n",topology_getMyNodeID());	
//	bid=0;
	//创建一个邻居表
	nt = nt_create();
	//将sip_conn初始化为-1, 即还未与SIP进程连接
	sip_conn = -1;
	
	//注册一个信号句柄, 用于终止进程
	signal(SIGINT, son_stop);

	//打印所有邻居
	int nbrNum = topology_getNbrNum();
//	nnum=nbrNum;
	int i;
	for(i=0;i<nbrNum;i++) {
		printf("Overlay network: neighbor %d:%d\n",i+1,nt[i].nodeID);
	}

	//启动waitNbrs线程, 等待节点ID比自己大的所有邻居的进入连接
	pthread_t waitNbrs_thread;
	pthread_create(&waitNbrs_thread,NULL,waitNbrs,(void*)0);

	//等待其他节点启动
	sleep(SON_START_DELAY);
	
	//连接到节点ID比自己小的所有邻居
	connectNbrs();

	//等待waitNbrs线程返回
	pthread_join(waitNbrs_thread,NULL);	

	//此时, 所有与邻居之间的连接都建立好了
	
	//创建线程监听所有邻居
	for(i=0;i<nbrNum;i++) {
		int* idx = (int*)malloc(sizeof(int));
		*idx = i;
		pthread_t nbr_listen_thread;
		pthread_create(&nbr_listen_thread,NULL,listen_to_neighbor,(void*)idx);
	}
	printf("Overlay network: node initialized...\n");
	printf("Overlay network: waiting for connection from SIP process...\n");
	//等待来自SIP进程的连接
	waitSIP();
}
