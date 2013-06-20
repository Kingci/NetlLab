
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "nbrcosttable.h"
#include "../common/constants.h"
#include "../topology/topology.h"

//这个函数动态创建邻居代价表并使用邻居节点ID和直接链路代价初始化该表.
//邻居的节点ID和直接链路代价提取自文件topology.dat. 
nbr_cost_entry_t* nbrcosttable_create()
{
	FILE *p;
	if((p = fopen("/home/b101220019/topology/topology.dat","r")) == NULL){
		printf("read error\n");
		return NULL;
	}
	char rs[30];
	while((fgets(rs,29,p)) != NULL){
		char delims[] = " ";
		char *host = NULL;
		int i = 0;
		int flag = 0;
		nbr_cost_entry_t *ncet = (nbr_cost_entry_t *)malloc(sizeof(nbr_cost_entry_t));
		for(host = strtok(rs, delims); host != NULL; i ++, host=strtok( NULL, delims)) {
			int hostID = host[9] - '0' + 184;
			switch(i){
				case 0:{
					if(hostID == topology_getMyNodeID()){
						flag = 1;
					}
					ncet->NodeID = hostID;
					break;
				}
				case 1:{
					printf( "hostname is %s\n", host );
					printf( "hostID is %d\n",hostID);
					ncet->Node2 = hostID;
					break;
				}
				case 2:{
					printf( "the cost of these hosts is %s\n", host );
					
					flag = 0;
					break;
				}
			}
		}  
	}
	return 0;
}

//这个函数删除邻居代价表.
//它释放所有用于邻居代价表的动态分配内存.
void nbrcosttable_destroy(nbr_cost_entry_t* nct)
{
	free(nct);
	return NULL;
}

//这个函数用于获取邻居的直接链路代价.
//如果邻居节点在表中发现,就返回直接链路代价.否则返回INFINITE_COST.
unsigned int nbrcosttable_getcost(nbr_cost_entry_t* nct, int nodeID)
{
	return INFINITE_COST;
}

//这个函数打印邻居代价表的内容.
void nbrcosttable_print(nbr_cost_entry_t* nct)
{
	return;
}
