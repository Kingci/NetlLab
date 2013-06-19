//文件名: topology/topology.c
//
//描述: 这个文件实现一些用于解析拓扑文件的辅助函数 
//
//创建日期: 2013年1月
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <assert.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include "topology.h"
#include "../common/constants.h"

//这个函数返回指定主机的节点ID.
//节点ID是节点IP地址最后8位表示的整数.
//例如, 一个节点的IP地址为202.119.32.12, 它的节点ID就是12.
//如果不能获取节点ID, 返回-1.
int hostID=0;
int topology_getNodeIDfromname(char* hostname) 
{	
	if(strcmp(hostname,"csnetlab_1") == 0) {
		hostID = 185;
		return hostID;
	}
	else if(strcmp(hostname,"csnetlab_2") == 0) {
		hostID = 186;
		return hostID;
	}
	else if(strcmp(hostname,"csnetlab_3") == 0) {
		hostID = 187;
		return hostID;
	}
	else if(strcmp(hostname,"csnetlab_4") == 0) {
		hostID = 188;
		return hostID;
	}
	else {
		hostID = -1;
		return hostID;
	}
}

//这个函数返回指定的IP地址的节点ID.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromip(struct in_addr* addr)
{
	return 0;
}

//这个函数返回本机的节点ID
//如果不能获取本机的节点ID, 返回-1.
int topology_getMyNodeID()
{
	char hostname[30];
	gethostname(hostname,sizeof(hostname));
	int neigh = topology_getNodeIDfromname(hostname);
	return neigh;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回邻居数.
int topology_getNbrNum()
{
	int num = 0;
	neigh_t *neighbor;
	for (neighbor = neighbort; neighbor!=NULL; neighbor=neighbor->next){
		if(neighbor->Node1 == hostID || neighbor->Node2 == hostID){
			num ++;
		}
	}
	return num;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回重叠网络中的总节点数.
int topology_getNodeNum()
{ 
	int node_num=0;
	neigh_t *neighbor;
	for(neighbor = neighbort; neighbor != NULL; neighbor = neighbor->next){
			node_num ++;
	}
	return node_num;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含重叠网络中所有节点的ID. 
int* topology_getNodeArray()
{
	int node_num=0;
	int *rs=(int *)malloc(20 * sizeof(int));
	neigh_t *neighbor;
	for(neighbor = neighbort; neighbor != NULL; neighbor = neighbor->next){
		if(node_num == 0){
			rs[0] = neighbor->Node1;
			rs[1] = neighbor->Node2;
			node_num = 2;
			continue;
		}
		int i;
		int flag = 0;
		for(i = 0; i < node_num; i++){
			if(neighbor->Node1 == rs[i]){
				flag = 1;
			}
		}
		if(flag == 0){
			rs[node_num ++] = neighbor->Node1;
			rs[node_num ++] = neighbor->Node2;
		}
	}
	return rs;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含所有邻居的节点ID.  
int* topology_getNbrArray()
{
	int *rs = (int *)malloc(20 * sizeof(int));
	int num = 0;
	neigh_t *neighbor;
	for (neighbor = neighbort; neighbor != NULL; neighbor = neighbor->next){
		if(neighbor->Node1 == hostID){
			rs[num ++] = neighbor->Node2;
		}
		if(neighbor->Node2 == hostID){
			rs[num ++] = neighbor->Node1;
		}
	}
	return rs;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回指定两个节点之间的直接链路代价. 
//如果指定两个节点之间没有直接链路, 返回INFINITE_COST.
unsigned int topology_getCost(int fromNodeID, int toNodeID)
{
	neigh_t *neighbor;
	for(neighbor = neighbort; neighbor != NULL; neighbor = neighbor->next){
		if((neighbor->Node1 == fromNodeID && neighbor->Node2 == toNodeID) || 
			(neighbor->Node2 == fromNodeID && neighbor->Node1 == toNodeID)){
			return neighbor->cost;
		}
	}
	return INFINITE_COST;
}
