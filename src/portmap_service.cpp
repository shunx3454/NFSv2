#include "portmap_service.h"
#include "utils.h"
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "rpc.h"

PortmapService::PortmapService(int port)
{
	// 注册 NFS 和 MOUNT 程序号 至 2049端口
	program_ports[100003] = 2049; // NFS 程序号
	program_ports[100005] = 2049; // MOUNT 程序号

	// 创建 UDP 套接字
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
	{
		throw std::runtime_error("Failed to create UDP socket: " + std::string(strerror(errno)));
	}

	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);

	if (bind(sockfd, (sockaddr *)&addr, sizeof(addr)) < 0)
	{
		throw std::runtime_error("Failed to bind portmap UDP socket: " + std::string(strerror(errno)));
	}
	std::cout << "Portmap Service start on port 111." << std::endl;
}

PortmapService::~PortmapService()
{
	close(sockfd);
}

void PortmapService::handle_portmap(void)
{
	int offset;
	char buffer[1024];
	sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);

	while (true)
	{
		ssize_t len = recvfrom(sockfd, buffer, sizeof(buffer), 0, (sockaddr *)&client_addr, &client_len);
		if (len < 36)
			std::cout << "GETPORT error length." << std::endl;

		// 解析 RPC 头部
		RPCHandle rpc_rx(buffer, len);
		rpc_rx.print(0);

		// 解析 GETPORT 参数
		PortMapInfo map;
		getmap(buffer + rpc_rx.RpcHeadSize, len - rpc_rx.RpcHeadSize, map);
		mapprint(map);

		// 只处理 UDP协议(17) 的 MOUNT(100003) 和 NFS(100003) 程序号的端口map
		int port;
		int reponsestate;

		auto it = program_ports.find(map.ProgramNum);
		if (it != program_ports.end() && map.Protocol == 17)
		{
			port = it->second;
			reponsestate = 0; // MSG_ACCEPTED
		}
		else
		{
			port = 0;
			reponsestate = 1; // MSG_DENIED
		}
		// 构造RPC响应头
		RPCHandle rpc_tx(rpc_rx.Xid, 1, reponsestate);
		rpc_tx.print(1);
		std::memcpy(buffer, &(rpc_tx.RPCbytes.at(0)), rpc_tx.RpcHeadSize);
		memww(buffer + rpc_tx.RpcHeadSize, htonl(port));

		if (sendto(sockfd, buffer, rpc_tx.RpcHeadSize + 4, 0, (sockaddr *)&client_addr, client_len) < 0)
			std::cout << "GETPORT response error." << std::endl;
		else
			std::cout << "GETPORT response success." << std::endl;
	}
}

void PortmapService::getmap(const char *data, int len, struct PortMapInfo &map)
{
	if (len < 16)
	{
		std::cout << "GETPORT error length data" << std::endl;
	}
	else
	{
		map.ProgramNum = ntohl(memrw(data));
		map.ProgramVer = ntohl(memrw(data + 4));
		map.Protocol = ntohl(memrw(data + 8));
		map.Port = ntohl(memrw(data + 12));
	}
}

void PortmapService::mapprint(const struct PortMapInfo &map)
{
	std::cout << std::endl
			  << "Portmap information"
			  << " Requst program: " << map.ProgramNum
			  << " Program Version: " << map.ProgramVer
			  << " Program Protocol: " << map.Protocol
			  << " Port: " << map.Port;
}
