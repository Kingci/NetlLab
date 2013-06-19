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
	printf("enter sonsendpkt\n");
  	int a=0;
	char *msg="!&";
	int len=2;
	a=send(son_conn,msg,len,0);
	if(a!=2){
		printf("senderror1\n");
		return -1;
	}
	len=pkt->header.length;
	sendpkt_arg_t *p= (sendpkt_arg_t *)malloc(sizeof(sendpkt_arg_t));
	p->nextNodeID=nextNodeID;
	printf("%d!!!!!!\n",p->nextNodeID);
	p->pkt=*pkt;
	printf("sendbuflen:%d",len);;
	a=send(son_conn,(void*)p,sizeof(sendpkt_arg_t),0);
	if(a!=sizeof(sendpkt_arg_t)){
		printf("senderror2\n");
		return -1;
	}
	char *msg2="!#";
	len=2;
	a=send(son_conn,(void*)msg2,len,0);
	if(a!=len){
		printf("senderror3\n");
		return -1;
	}
	printf("end sonsendpkt\n");
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
	//printf("enter son_recvpkt\n");
 	int state=0;
	char buffer[1600];
	int i=0;
	char buf;
	int t;
	while(1){
		t=recv(son_conn,&buf,1,0);
		if(t<=0)
			break;
		switch(state){
		case 0:
			if(buf=='!')
				state=1;
			break;
		case 1:
			if(buf=='&')
				state=2;
			break;
		case 2:
			if(buf=='!'){
				buffer[i++]=buf;
				t=recv(son_conn,&buf,1,0);
				if(buf=='#'){
					state=4;
					i--;
				}
				else{
					buffer[i++]=buf;
				}
			}
			else
				buffer[i++]=buf;
			break;
		case 3:
			if(buf=='#')
				state=4;
			break;
		case 4:
			break;
		}
		if(state==4)
			break;
		
	}
	if(state!=4){

		printf("%d",state);
		printf("wrong\n");
		return -1;
	}
	//sendpkt_arg_t *p=(sip_pkt_t*)&buffer;
	memcpy(pkt,&buffer,i);
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
	printf("enter getpktToSend\n");
  	int state=0;
	char buffer[1600];
	int i=0;
	char buf;
	int t;
	while(1){
		t=recv(sip_conn,&buf,1,0);
		if(t<=0)
			break;
		switch(state){
		case 0:
			if(buf=='!')
				state=1;
			break;
		case 1:
			if(buf=='&')
				state=2;
			break;
		case 2:
			if(buf=='!'){
				buffer[i++]=buf;
				t=recv(sip_conn,&buf,1,0);
				if(buf=='#'){
					state=4;
					i--;
				}
				else{
					buffer[i++]=buf;
				}
			}
			else
				buffer[i++]=buf;
			break;
		case 3:
			if(buf=='#')
				state=4;
			break;
		case 4:
			break;
		}
		if(state==4)
			break;
		
	}
	if(state!=4){
		return -1;
	}
	sendpkt_arg_t *p=(sendpkt_arg_t *)&buffer;
	*pkt=p->pkt;
	*nextNode=p->nextNodeID;
	printf("end getpktToSend\n");
  return 1;
}

// forwardpktToSIP()函数是在SON进程接收到来自重叠网络中其邻居的报文后被调用的. 
// SON进程调用这个函数将报文转发给SIP进程. 
// 参数sip_conn是SIP进程和SON进程之间的TCP连接的套接字描述符. 
// 报文通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#, 按照'!& 报文 !#'的顺序发送. 
// 如果报文发送成功, 返回1, 否则返回-1.
int forwardpktToSIP(sip_pkt_t* pkt, int sip_conn)
{
	printf("enter forwardpktToSIP\n");
    int a=0;
	char *msg="!&";
	int len=2;
	a=send(sip_conn,msg,len,0);
	if(a!=2){
		printf("senderror1\n");
		return -1;
	}
	len=pkt->header.length;
	/*sip_pkt_t *p= (sip_pkt_t *)malloc(sizeof(sip_pkt_t));
	p=*pkt;*/
	printf("sendbuflen:%d",len);
	a=send(sip_conn,(void*)pkt,sizeof(sip_pkt_t),0);
	if(a!=sizeof(sip_pkt_t)){
		printf("senderror2\n");
		return -1;
	}
	char *msg2="!#";
	len=2;
	a=send(sip_conn,(void*)msg2,len,0);
	if(a!=len){
		printf("senderror3\n");
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
	printf("enter sendpkt\n");
    int a=0;
	char *msg="!&";
	int len=2;
	a=send(conn,msg,len,0);
	if(a!=2){
		printf("senderror1\n");
		return -1;
	}
	len=pkt->header.length;
	/*sip_pkt_t *p= (sip_pkt_t *)malloc(sizeof(sip_pkt_t));
	p=*pkt;*/
	printf("sendbuflen:%d",len);
	a=send(conn,(void*)pkt,sizeof(sip_pkt_t),0);
	if(a!=sizeof(sip_pkt_t)){
		printf("senderror2\n");
		return -1;
	}
	char *msg2="!#";
	len=2;
	a=send(conn,(void*)msg2,len,0);
	if(a!=len){
		printf("senderror3\n");
		return -1;
	}
	printf("end sendpkt\n");
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
	//printf("enter recvpkt\n");
   	int state=0;
	char buffer[1600];
	int i=0;
	char buf;
	int t;
	while(1){
		t=recv(conn,&buf,1,0);
		if(t<=0)
			return -1;
		switch(state){
		case 0:
			if(buf=='!')
				state=1;
			break;
		case 1:
			if(buf=='&')
				state=2;
			break;
		case 2:
			if(buf=='!'){
				buffer[i++]=buf;
				t=recv(conn,&buf,1,0);
				if(buf=='#'){
					state=4;
					i--;
				}
				else{
					buffer[i++]=buf;
				}
			}
			else
				buffer[i++]=buf;
			break;
		case 3:
			if(buf=='#')
				state=4;
			break;
		case 4:
			break;
		}
		if(state==4)
			break;
		
	}
	if(state!=4){

		printf("%d",state);
		printf("wrong\n");
		return -1;
	}
	//sendpkt_arg_t *p=(sip_pkt_t*)&buffer;
	printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!2121212\n");
	memcpy(pkt,&buffer,i);
	printf("end recvpkt\n");
  return 1;
}
