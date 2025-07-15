#include "portmap_service.h"
#include "nfs_service.h"
#include "utils.h"
#include <thread>
#include <iostream>

int main(int argv, char *argc[])
{
	if (argv != 2)
	{
		std::cout << "Usage: " << argc[0] << " /mount/dir/" << std::endl;
		return 0;
	}

	// 启动 Portmap 服务（UDP）, 端口 111
	PortmapService portmap(111);
	std::thread portmap_thread(&PortmapService::handle_portmap, &portmap);
	portmap_thread.detach();

	// 启动 NFS/MOUNT 服务器， 端口2049
	NFSService server(argc[1], 2049);
	server.run();

	return 0;
}