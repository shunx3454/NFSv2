#include <sys/stat.h>
#include "nfs_service.h"
#include "utils.h"
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <iostream>
#include <functional>
#include <algorithm>
#include <memory>
#include "rpc.h"

NFSService::NFSService(const std::string &path, int server_port) : nfs_dir(path)
{
	// 初始化 NFS 服务器（UDP）
	server_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (server_fd < 0)
	{
		std::cout << "NFS socket error" << std::endl;
	}
	sockaddr_in addr_nfs;
	addr_nfs.sin_family = AF_INET;
	addr_nfs.sin_addr.s_addr = INADDR_ANY;
	addr_nfs.sin_port = htons(server_port);
	if (bind(server_fd, (sockaddr *)&addr_nfs, sizeof(addr_nfs)) < 0)
	{
		std::cout << "NFS can not bind the port: " << server_port << std::endl;
	}
	std::cout << "NFS/MOUNT Service start on port " << server_port << std::endl;
}

NFSService::~NFSService()
{
	close(server_fd);
}

size_t string_hash(const std::string &s)
{
	std::hash<std::string> hasher;
	return hasher(s);
}

void write_file_handle(char *data, int len, size_t hs)
{
	memset(data, 0, len);
	memcpy(data, &hs, sizeof(size_t));
}

size_t read_file_handle(const char *data, int len)
{
	size_t hs;
	memcpy(&hs, data, sizeof(size_t));
	return hs;
}

void set_fattr(char *src, struct stat *ft)
{
	int offset = 0;
	// fattr 68 bytes
	if (ft->st_mode & S_IFMT == S_IFREG)
		memww(src, htonl(1)); // Type = FILE
	else if (ft->st_mode & S_IFMT == S_IFDIR)
		memww(src, htonl(2)); // Type = DIR
	else
		memww(src, htonl(3)); // Type = BLK
	offset += 4;

	memww(src + offset, htonl(ft->st_mode & 0777)); // 仅用户读写
	offset += 4;
	memww(src + offset, htonl(ft->st_nlink));
	offset += 4;
	memww(src + offset, htonl(ft->st_uid));
	offset += 4;
	memww(src + offset, htonl(ft->st_gid));
	offset += 4;
	memww(src + offset, htonl(ft->st_size));
	offset += 4;
	memww(src + offset, htonl(ft->st_blksize));
	offset += 4;
	memww(src + offset, htonl(ft->st_rdev));
	offset += 4;
	memww(src + offset, htonl(ft->st_blocks));
	offset += 4;
	memww(src + offset, htonl(ft->st_dev));
	offset += 4;
	memww(src + offset, htonl(ft->st_ino));
	offset += 4;
	memww(src + offset, htonl(ft->st_atim.tv_sec));
	offset += 4;
	memww(src + offset, htonl(ft->st_atim.tv_nsec));
	offset += 4;
	memww(src + offset, htonl(ft->st_mtim.tv_sec));
	offset += 4;
	memww(src + offset, htonl(ft->st_mtim.tv_nsec));
	offset += 4;
	memww(src + offset, htonl(ft->st_ctim.tv_sec));
	offset += 4;
	memww(src + offset, htonl(ft->st_ctim.tv_nsec));
	offset += 4;
}

void NFSService::mount(RPCHandle &rpc, const char *data, int len, const sockaddr_in &client_addr)
{
	std::string mount_path;
	size_t dir_hash;
	char reply[1024];
	int mountstate;
	try
	{
		// 解析路径
		uint32_t path_len = ntohl(memrw(data));
		mount_path.assign(data + 4, path_len);
		// 查看路径是否存在
		struct stat st;
		if (stat(mount_path.c_str(), &st) == 0 && (st.st_mode & S_IFDIR))
		{
			// 这里文件句柄 用hash值代替
			dir_hash = string_hash(mount_path);
			nfs_fhandle.insert(std::make_pair(mount_path, dir_hash));
			mountstate = 0; // MNT_OK
		}
		else
		{
			std::cout << "NFS MOUNT path not exist." << std::endl;
			mountstate = 13; // MNT_ERR_ACCESS
		}
	}
	catch (std::exception &e)
	{
		std::cout << "NFS MOUNT exception:" << e.what() << std::endl;
		mountstate = 13; // MNT_ERR_ACCESS
	}
	// 记录文件句柄
	// 构造RPC响应头
	RPCHandle rpc_tx(rpc.Xid);
	std::memcpy(reply, &(rpc_tx.RPCbytes.at(0)), rpc_tx.RpcHeadSize);
	// MOUNT状态写入
	memww(reply + rpc_tx.RpcHeadSize, htonl(mountstate));
	rpc_tx.RpcHeadSize += 4;
	if (mountstate == 0)
	{
		// file handle
		write_file_handle(reply + rpc_tx.RpcHeadSize, 32, dir_hash);
		rpc_tx.RpcHeadSize += 32;
	}

	if (sendto(server_fd, reply, rpc_tx.RpcHeadSize, 0, (sockaddr *)&client_addr, sizeof(client_addr)) < 0)
		std::cout << "NFS MOUNT response error" << std::endl;
	else
		std::cout << "NFS MOUNT response success" << std::endl;
}

void NFSService::umntall(RPCHandle &rpc, const char *data, int len, const sockaddr_in &client_addr)
{
	char reply[1024];
	std::memset(reply, 0, sizeof(reply));
	RPCHandle rpc_tx(rpc.Xid);
	std::memcpy(reply, &(rpc_tx.RPCbytes.at(0)), rpc_tx.RpcHeadSize);

	struct stat st;
	for (auto it = nfs_fhandle.begin(); it != nfs_fhandle.end();)
	{
		if (stat(it->first.c_str(), &st) == 0)
		{
			if (st.st_mode & S_IFREG)
			{
				fclose((FILE *)it->second);
			}
			it = nfs_fhandle.erase(it);
		}
		else
		{
			it++;
		}
	}

	// UDP reply
	if (sendto(server_fd, reply, rpc_tx.RpcHeadSize, 0, (sockaddr *)&client_addr, sizeof(client_addr)) < 0)
		std::cout << "NFS UMNTALL response error" << std::endl;
	else
		std::cout << "NFS UMNTALL response success" << std::endl;
}

void NFSService::read(RPCHandle &rpc, const char *data, int len, const sockaddr_in &client_addr)
{
	int errorcode = 0;
	uint32_t offset = 0;
	uint32_t read_offset = 0;
	uint32_t read_len = 0;
	size_t fh;
	char reply[1024];

	fh = read_file_handle(data, 32);
	read_offset = ntohl(memrw(data + 32));
	read_len = ntohl(memrw(data + 36));

	// 构造RPC 头
	RPCHandle rpc_tx(rpc.Xid);
	std::memcpy(reply, &(rpc_tx.RPCbytes.at(0)), rpc_tx.RpcHeadSize);
	offset += rpc_tx.RpcHeadSize;

	auto it = std::find_if(nfs_fhandle.begin(), nfs_fhandle.end(),
						   [&fh](const auto &pair)
						   {
							   return pair.second == fh;
						   });

	if (it != nfs_fhandle.end())
	{
		std::string full_path = it->first;
		struct stat st;
		if (stat(full_path.c_str(), &st) == 0)
		{
			// status
			memww(reply + offset, htonl(0)); // NFS_OK
			offset += 4;

			// fattr
			set_fattr(reply + offset, &st);
			offset += 68;

			// data len align 4 bytes
			uint32_t add_oquaue_len = (offset + 4 + read_len + 3) & ~(3);
			std::unique_ptr<char> reply_data(new char[add_oquaue_len]);
			memset(reply_data.get(), 0, add_oquaue_len);

			// read file
			FILE *fl = (FILE *)fh;
			fseek(fl, read_offset, SEEK_SET);
			read_len = fread(reply_data.get() + offset + 4, 1, read_len, fl); // reserve 4 bytes for describing the data length

			// data align 4 bytes
			add_oquaue_len = (offset + 4 + read_len + 3) & ~(3);
			memww(reply_data.get() + offset, htonl(read_len));

			// copy RPC Head
			memcpy(reply_data.get(), reply, offset);
			// UDP reply
			if (sendto(server_fd, reply_data.get(), add_oquaue_len, 0, (sockaddr *)&client_addr, sizeof(client_addr)) < 0)
				std::cout << "NFS READ response error" << std::endl;
			else
				std::cout << "NFS READ response success" << std::endl;
		}
		else
		{
			errorcode = 1;
			std::cout << "NFS READ stat error." << std::endl;
		}
	}
	else
	{
		errorcode = 2;
		std::cout << "NFS READ no file handle." << std::endl;
	}

	if (errorcode)
	{
		// status
		memww(reply + offset, htonl(2)); // NFS_ERR
		offset += 4;

		// UDP reply
		if (sendto(server_fd, reply, offset, 0, (sockaddr *)&client_addr, sizeof(client_addr)) < 0)
			std::cout << "NFS READ response(error: " << errorcode << ") error" << std::endl;
		else
			std::cout << "NFS READ response(error: " << errorcode << ") success" << std::endl;
	}
}

void NFSService::lookup(RPCHandle &rpc, const char *data, int len, const sockaddr_in &client_addr)
{
	uint32_t offset = 0;
	int errorcode = 0;
	char reply[1024];
	uint32_t flen;
	size_t path_hash = read_file_handle(data, 32);
	memcpy(&flen, data + 32, 4);
	flen = ntohl(flen);
	std::string flname(data + 36, flen);

	// 构造RPC 头
	RPCHandle rpc_tx(rpc.Xid);
	offset += rpc_tx.RpcHeadSize;
	std::memcpy(reply, &(rpc_tx.RPCbytes.at(0)), offset);

	// 查找访问路径hash值是否存在
	auto it = std::find_if(nfs_fhandle.begin(), nfs_fhandle.end(),
						   [&path_hash](const auto &pair)
						   {
							   return pair.second == path_hash;
						   });
	if (it != nfs_fhandle.end())
	{
		struct stat st;
		std::string file_path = it->first + "/" + flname;
		if (stat(it->first.c_str(), &st) == 0)
		{
			// file handle 32 bytes
			FILE *fh = fopen(file_path.c_str(), "rb");
			if (fh != NULL)
			{
				// status
				memww(reply + offset, htonl(0)); // NFS_OK
				offset += 4;

				write_file_handle(reply + offset, 32, (size_t)fh);
				nfs_fhandle.insert(std::make_pair(file_path, (size_t)fh)); // 记录文件FILE*
				offset += 32;

				// file attr 68 bytes
				set_fattr(reply + offset, &st);
				offset += 68;
			}
			else
			{
				errorcode = 1;
				std::cout << "NFS LOOKUP:" << flname << " open error" << std::endl;
			}
		}
		else
		{
			errorcode = 2;
			std::cout << "NFS LOOKUP:" << flname << " stat error" << std::endl;
		}
	}
	else
	{
		errorcode = 3;
		std::cout << "NFS LOOKUP no file: " << flname << std::endl;
	}
	if (errorcode)
	{
		// status
		memww(reply + offset, htonl(2)); // NFS_ERR
		offset += 4;
	}

	// UDP reply
	if (sendto(server_fd, reply, offset, 0, (sockaddr *)&client_addr, sizeof(client_addr)) < 0)
	{
		std::cout << "NFS LOOKUP response error" << std::endl;
	}
	else
	{
		std::cout << "NFS LOOKUP response success" << std::endl;
	}
}

void NFSService::run(void)
{
	// 处理 NFS
	char buffer[1024];
	sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);

	while (true)
	{
		ssize_t len = recvfrom(server_fd, buffer, sizeof(buffer), 0, (sockaddr *)&client_addr, &client_len);
		if (len > 0)
		{
			handle_nfs(buffer, len, client_addr, client_len);
		}
	}
}

void NFSService::handle_nfs(const char *buffer, ssize_t len, const sockaddr_in &client_addr, socklen_t client_len)
{
	// 解析 RPC 头部
	RPCHandle rpc_rx(buffer, len);
	rpc_rx.print(0);

	// NFS 处理
	if (rpc_rx.ProgramNnum == 100003)
	{
		switch (rpc_rx.ProcessNum)
		{
		case 4: // LOOKUP Call
			lookup(rpc_rx, buffer + rpc_rx.RpcHeadSize, len - rpc_rx.RpcHeadSize, client_addr);
			break;

		case 6: // READ Call
			read(rpc_rx, buffer + rpc_rx.RpcHeadSize, len - rpc_rx.RpcHeadSize, client_addr);
			break;

		default:
			break;
		}
	}
	// MOUNT 处理
	else if (rpc_rx.ProgramNnum == 100005)
	{
		switch (rpc_rx.ProcessNum)
		{
		case 1: // MOUNT Call
			mount(rpc_rx, buffer + rpc_rx.RpcHeadSize, len - rpc_rx.RpcHeadSize, client_addr);
			break;
		case 4: // UMNTALL Call
			umntall(rpc_rx, buffer + rpc_rx.RpcHeadSize, len - rpc_rx.RpcHeadSize, client_addr);
			break;

		default:
			break;
		}
	}
	else
	{
		std::cout << "NFS invalide program: " << rpc_rx.ProcessNum << std::endl;
	}
}
