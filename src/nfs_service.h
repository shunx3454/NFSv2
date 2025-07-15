#ifndef NFS_SERVICE_H
#define NFS_SERVICE_H

#include <string>
#include <map>
#include <vector>
#include <netinet/in.h>
#include "portmap_service.h"
#include "rpc.h"

class NFSService {
private:
	int server_fd;
    std::string nfs_dir;
    std::map<std::string, size_t> nfs_fhandle;
	void mount(RPCHandle &rpc, const char *data, int len, const sockaddr_in &client_addr);
	void umntall(RPCHandle &rpc, const char *data, int len, const sockaddr_in &client_addr);
	void lookup(RPCHandle &rpc, const char *data, int len, const sockaddr_in &client_addr);
	void read(RPCHandle &rpc, const char *data, int len, const sockaddr_in &client_addr);
	void handle_nfs(const char *buffer, ssize_t len, const sockaddr_in &client_addr, socklen_t client_len);

public:
    NFSService(const std::string& path, int server_port);
    ~NFSService();
	void run(void);
};

#endif