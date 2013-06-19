//文件名: son/neighbortable.c
//
//描述: 这个文件实现用于邻居表的API
//
//创建日期: 2013年1月

#include "neighbortable.h"

//这个函数首先动态创建一个邻居表. 然后解析文件topology/topology.dat, 填充所有条目中的nodeID和nodeIP字段, 将conn字段初始化为-1.
//返回创建的邻居表.
nbr_entry_t* nt_create()
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
		neigh_t *nh = (neigh_t *)malloc(sizeof(neigh_t));
		for(host = strtok(rs, delims); host != NULL; i ++, host=strtok( NULL, delims)) {
			int hostID = host[9] - '0' + 184;
			switch(i){
				case 0:{
					printf( "hostname is %s\n", host );
					printf( "hostID is %d\n", hostID);
					nh->Node1 = hostID;
					break;
				}
				case 1:{
					printf( "hostname is %s\n", host );
					printf( "hostID is %d\n",hostID);
					nh->Node2 = hostID;
					break;
				}
				case 2:{
					printf( "the cost of these hosts is %s\n", host );
					nh->cost = host[0] - '0';
					nh->next = neighbort;
					neighbort = nh;
					break;
				}
			}
		}  
	}
	
	neighNum = topology_getNbrNum();
	printf("the num of neibors is %d\n\n", neighNum);
	nbr_entry_t * neighEntry = (nbr_entry_t *)malloc(sizeof(nbr_entry_t) * neighNum);
	int *neighArray = topology_getNbrArray();
	int i =0;
	while(i < neighNum){
		neighEntry[i].nodeID = neighArray[i];
		in_addr_t addr;
		switch (neighArray[i]){
			case 185: addr = inet_addr("114.212.190.185");
				break;
			case 186: addr = inet_addr("114.212.190.186"); 
				break;
			case 187: addr = inet_addr("114.212.190.187"); 
				break;
			case 188: addr = inet_addr("114.212.190.188");
				break;
			default: break;
		}
		neighEntry[i].nodeIP = addr;
		neighEntry[i].conn = -1;
		i ++;
	}
	return neighEntry;
}
//这个函数删除一个邻居表. 它关闭所有连接, 释放所有动态分配的内存.
void nt_destroy(nbr_entry_t* nt)
{
	free(nt);
	return;
}

//这个函数为邻居表中指定的邻居节点条目分配一个TCP连接. 如果分配成功, 返回1, 否则返回-1.
int nt_addconn(nbr_entry_t* nt, int nodeID, int conn)
{
	FILE *p;
	if((p = fopen("/home/b101220019/topology/topology.dat","r")) == NULL){
		printf("read error\n");
		return -1;
	}
	char rs[30];
	while((fgets(rs,29,p)) != NULL){
		char delims[] = " ";
		char *host = NULL;
		int i = 0;
		neigh_t *nh = (neigh_t *)malloc(sizeof(neigh_t));
		for(host = strtok(rs, delims); host != NULL; i ++, host=strtok( NULL, delims)) {
			int hostID = host[9] - '0' + 184;
			if(i == 0)
				nh->Node1 = hostID;
			if(i == 1)
				nh->Node2 = hostID;
			if(i == 2){
				nh->cost = host[0] - '0';
				nh->next = neighbort;
				neighbort = nh;
			}
		}  
	}
	
	neighNum = topology_getNbrNum();
	nbr_entry_t * neighEntry = (nbr_entry_t *)malloc(sizeof(nbr_entry_t) * neighNum);
	int *neighArray = topology_getNbrArray();
	int i =0;
	while(i < neighNum){
		neighEntry[i].nodeID = neighArray[i];
		i ++;
	}
	return 1;
}
