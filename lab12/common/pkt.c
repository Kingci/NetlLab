// 文件名: common/pkt.c
// 创建日期: 2013年1月

#include "pkt.h"

// son_sendpkt()由SIP进程调用, 其作用是要求SON进程将报文发送到重叠网络中. SON进程和SIP进程通过一个本地TCP连接互连.
// 在son_sendpkt()中, 报文及其下一跳的节点ID被封装进数据结构sendpkt_arg_t, 并通过TCP连接发送给SON进程. 
// 参数son_conn是SIP进程和SON进程之间的TCP连接套接字描述符.
// 当通过SIP进程和SON进程之间的TCP连接发送数据结构sendpkt_arg_t时, 使用'!&'和'!#'作为分隔符, 按照'!& sendpkt_arg_t结构 !#'的顺序发送.
// 如果发送成功, 返回1, 否则返回-1.
int son_sendpkt(int nextNodeID, sip_pkt_t* pkt, int son_conn)
{
	printf("***********************SIP send packet to SON**********************\n");
	if (send(son_conn, "!&", 2, 0) < 0) {
		perror("Error in sending data(son_sendpkt)\n");
		return -1;
	}
//	printf("sendbuflen:%d",pkt->header.length);
	sendpkt_arg_t *packet = (sendpkt_arg_t *)malloc(sizeof(sendpkt_arg_t));
	packet->nextNodeID = nextNodeID;
	packet->pkt = *pkt;
	if(send(son_conn, (void*)packet, sizeof(sendpkt_arg_t), 0) < 0){
		perror("Error in sending data(son_sendpkt)\n");
		return -1;
	}
	
	if (send(son_conn, "!#", 2, 0) < 0) {
		perror("Error in sending data(son_sendpkt)\n");
		return -1;
	}
	return 1;
}

// son_recvpkt()函数由SIP进程调用, 其作用是接收来自SON进程的报文. 
// 参数son_conn是SIP进程和SON进程之间TCP连接的套接字描述符. 报文通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#. 
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点 
// PKTSTART2 -- 接收到'!', 期待'&' 
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收 
// 如果成功接收报文, 返回1, 否则返回-1.
int son_recvpkt(sip_pkt_t* pkt, int son_conn)
{
	printf("***********************SIP receive packet from SON**********************\n");
  	int state=0;
	char buffer[1600];
	int pos=0;
	char buf;
	while(1){
		if(recv(son_conn,&buf,1,0)<=0)
			break;
		switch(state){
			case 0:
				if(buf == '!')
					state = 1;
				break;
			case 1:
				if(buf == '&')
					state = 2;
				break;
			case 2:
				if(buf == '!'){
					//如果是#结束，如果！后面不是#则证明是所要传输的数据
					buffer[pos ++] = buf;
					recv(son_conn, &buf, 1, 0);
					if(buf == '#'){
						state = 4;
						pos --;
					}
					else
						buffer[pos ++] = buf;
				}
				else
					buffer[pos ++] = buf;
				break;
			case 3:
				if(buf == '#')
					state = 4;
				break;
		}
		if(state == 4)
			break;
	}
	if(state != 4){
		return -1;
	}
	//sendpkt_arg_t *packet=(sip_pkt_t*)&buffer;
	memcpy(pkt, &buffer, pos);
	return 1;
}

// 这个函数由SON进程调用, 其作用是接收数据结构sendpkt_arg_t.
// 报文和下一跳的节点ID被封装进sendpkt_arg_t结构.
// 参数sip_conn是在SIP进程和SON进程之间的TCP连接的套接字描述符. 
// sendpkt_arg_t结构通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#. 
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点 
// PKTSTART2 -- 接收到'!', 期待'&' 
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收
// 如果成功接收sendpkt_arg_t结构, 返回1, 否则返回-1.
int getpktToSend(sip_pkt_t* pkt, int* nextNode,int sip_conn)
{
	printf("***********************SON receive packet from SIP**********************\n");
  	int state=0;
	char buffer[1600];
	int pos=0;
	char buf;
	while(1){
		if(recv(sip_conn,&buf,1,0)<=0)
			break;
		switch(state){
			case 0:
				if(buf == '!')
					state = 1;
				break;
			case 1:
				if(buf == '&')
					state = 2;
				break;
			case 2:
				if(buf == '!'){
					//如果是#结束，如果！后面不是#则证明是所要传输的数据
					buffer[pos ++] = buf;
					recv(sip_conn, &buf, 1, 0);
					if(buf == '#'){
						state = 4;
						pos --;
					}
					else
						buffer[pos ++] = buf;
				}
				else
					buffer[pos ++] = buf;
				break;
			case 3:
				if(buf == '#')
					state = 4;
				break;
		}
		if(state == 4)
			break;
	}
	if(state != 4){
		return -1;
	}
	sendpkt_arg_t *packet = (sendpkt_arg_t *)&buffer;
	*pkt = packet->pkt;
	*nextNode = packet->nextNodeID;
//	printf("end getpktToSend\n");
	return 1;
}

// forwardpktToSIP()函数是在SON进程接收到来自重叠网络中其邻居的报文后被调用的. 
// SON进程调用这个函数将报文转发给SIP进程. 
// 参数sip_conn是SIP进程和SON进程之间的TCP连接的套接字描述符. 
// 报文通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#, 按照'!& 报文 !#'的顺序发送. 
// 如果报文发送成功, 返回1, 否则返回-1.
int forwardpktToSIP(sip_pkt_t* pkt, int sip_conn)
{
	printf("***********************SON send packet to SIP**********************\n");
    if (send(sip_conn, "!&", 2, 0) < 0) {
		perror("Error in sending data(forwardpktToSIP)\n");
		return -1;
	}	
//	printf("sendbuflen:%d",pkt->header.length);
	if(send(sip_conn,(void*)pkt,sizeof(sip_pkt_t),0) < 0){
		printf("Error in sending data(forwardpktToSIP)\n");
		return -1;
	}
	if (send(sip_conn, "!#", 2, 0) < 0) {
		perror("Error in sending data(forwardpktToSIP)\n");
		return -1;
	}
	return 1;
}

// sendpkt()函数由SON进程调用, 其作用是将接收自SIP进程的报文发送给下一跳.
// 参数conn是到下一跳节点的TCP连接的套接字描述符.
// 报文通过SON进程和其邻居节点之间的TCP连接发送, 使用分隔符!&和!#, 按照'!& 报文 !#'的顺序发送. 
// 如果报文发送成功, 返回1, 否则返回-1.
int sendpkt(sip_pkt_t* pkt, int conn)
{
	printf("***********************SON send packet to Next Hop**********************\n");

	if (send(conn, "!&", 2, 0) < 0) {
		perror("Error in sending data(sendpkt)\n");
		return -1;
	}	
//	printf("sendbuflen:%d",pkt->header.length);
	if(send(conn,(void*)pkt, sizeof(sip_pkt_t), 0) < 0){
		printf("Error in sending data(sendpkt)\n");
		return -1;
	}
	if (send(conn, "!#", 2, 0) < 0) {
		perror("Error in sending data(sendpkt)\n");
		return -1;
	}
	return 1;
}

// recvpkt()函数由SON进程调用, 其作用是接收来自重叠网络中其邻居的报文.
// 参数conn是到其邻居的TCP连接的套接字描述符.
// 报文通过SON进程和其邻居之间的TCP连接发送, 使用分隔符!&和!#. 
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点 
// PKTSTART2 -- 接收到'!', 期待'&' 
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收 
// 如果成功接收报文, 返回1, 否则返回-1.
int recvpkt(sip_pkt_t* pkt, int conn)
{
	printf("***********************SON receive packet from Next Hop**********************\n");
  	int state=0;
	char buffer[1600];
	int pos=0;
	char buf;
	while(1){
		if(recv(conn,&buf,1,0)<=0)
			break;
		switch(state){
			case 0:
				if(buf == '!')
					state = 1;
				break;
			case 1:
				if(buf == '&')
					state = 2;
				break;
			case 2:
				if(buf == '!'){
					//如果是#结束，如果！后面不是#则证明是所要传输的数据
					buffer[pos ++] = buf;
					recv(conn, &buf, 1, 0);
					if(buf == '#'){
						state = 4;
						pos --;
					}
					else
						buffer[pos ++] = buf;
				}
				else
					buffer[pos ++] = buf;
				break;
			case 3:
				if(buf == '#')
					state = 4;
				break;
		}
		if(state == 4)
			break;
	}
	if(state != 4){
		return -1;
	}
	//sendpkt_arg_t *packet=(sip_pkt_t*)&buffer;
//	printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!2121212\n");
	memcpy(pkt, &buffer, pos);
//	printf("end recvpkt\n");
	return 1;
}
