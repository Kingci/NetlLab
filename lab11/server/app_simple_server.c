//�ļ���: server/app_simple_server.c

//����: ���Ǽ򵥰汾�ķ������������. ����������ͨ���ڿͻ��˺ͷ�����֮�䴴��TCP����,�����ص������. Ȼ��������stcp_server_init()��ʼ��STCP������. 
//��ͨ�����ε���stcp_server_sock()��stcp_server_accept()����2���׽��ֲ��ȴ����Կͻ��˵�����. ������Ȼ����������������ӵĿͻ��˷��͵Ķ��ַ���. 
//���, ������ͨ������stcp_server_close()�ر��׽���. �ص������ͨ������son_stop()ֹͣ.

//��������: 2013��1��

//����: ��

//���: STCP������״̬

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>

#include "../common/constants.h"
#include "stcp_server.h"

//������������, һ��ʹ�ÿͻ��˶˿ں�87�ͷ������˿ں�88. ��һ��ʹ�ÿͻ��˶˿ں�89�ͷ������˿ں�90.
#define CLIENTPORT1 87
#define SERVERPORT1 88
#define CLIENTPORT2 89
#define SERVERPORT2 90
//�ڽ��յ��ַ�����, �ȴ�10��, Ȼ��ر�����.
#define WAITTIME 10
#define LISTENQ 2
//�������ͨ���ڿͻ��ͷ�����֮�䴴��TCP�����������ص������. ������TCP�׽���������, STCP��ʹ�ø����������Ͷ�. ���TCP����ʧ��, ����-1.
int son_start() {
	int listenfd = socket(AF_INET, SOCK_STREAM, 0);	//����socket
	if (listenfd < 0) {
		perror("Server: Creating socket wrong !!\n");
		return -1;
	}
	
	struct sockaddr_in servaddr;
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(8000);
	int b = bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));	//��
	if (b < 0) {
		perror("Server: Binding wrong !!\n");
		return -1;
	}
	
	listen(listenfd, MAX_TRANSPORT_CONNECTIONS);	//�����˿�
	
	struct sockaddr_in cliaddr;
	socklen_t clilen = sizeof(cliaddr);
	int connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen); 
	printf("In son_start(), get a request from client, connfd is %d\n", connfd);
	return connfd;

}

//�������ͨ���رտͻ��ͷ�����֮���TCP������ֹͣ�ص������
void son_stop(int son_conn) {
	close(son_conn);
  //����Ҫ��д����Ĵ���.

}

int main() {
	//���ڶ����ʵ����������
	srand(time(NULL));

	//�����ص�����㲢��ȡ�ص������TCP�׽���������
	int son_conn = son_start();
	if(son_conn<0) {
		printf("can not start overlay network\n");
	}

	//��ʼ��STCP������
	stcp_server_init(son_conn);

	//�ڶ˿�SERVERPORT1�ϴ���STCP�������׽��� 
	int sockfd= stcp_server_sock(SERVERPORT1);
	if(sockfd<0) {
		printf("can't create stcp server\n");
		exit(1);
	}
	//��������������STCP�ͻ��˵����� 
	stcp_server_accept(sockfd);

	//�ڶ˿�SERVERPORT2�ϴ�����һ��STCP�������׽���
	int sockfd2= stcp_server_sock(SERVERPORT2);
	if(sockfd2<0) {
		printf("can't create stcp server\n");
		exit(1);
	}
	//��������������STCP�ͻ��˵����� 
	stcp_server_accept(sockfd2);

	char buf1[6];
	char buf2[7];
	int i;
	//�������Ե�һ�����ӵ��ַ���
	for(i=0;i<5;i++) {
		stcp_server_recv(sockfd,buf1,6,i);
		printf("recv string: %s from connection 1!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n",buf1);
	}
	//�������Եڶ������ӵ��ַ���
	for(i=0;i<5;i++) {
		stcp_server_recv(sockfd2,buf2,7,5+i);
		printf("recv string: %s from connection 2\n",buf2);
	}

	sleep(WAITTIME);

	//�ر�STCP������ 
	if(stcp_server_close(sockfd)<0) {
		printf("can't destroy stcp server\n");
		exit(1);
	}				
	if(stcp_server_close(sockfd2)<0) {
		printf("can't destroy stcp server\n");
		exit(1);
	}				

	//ֹͣ�ص������
	son_stop(son_conn);
}