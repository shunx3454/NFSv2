#ifndef PORTMAP_SERVICE_H
#define PORTMAP_SERVICE_H

#include <map>
#include <netinet/in.h>
#include <string>

struct PortMapInfo
{
	int ProgramNum;
	int ProgramVer;
	int Protocol;
	int Port;
};

class PortmapService
{
private:
	int sockfd;
	std::map<uint32_t, uint16_t> program_ports;

public:
	PortmapService(int port);
	~PortmapService();
	void handle_portmap();
	void getmap(const char *data, int len, struct PortMapInfo &map);
	void mapprint(const struct PortMapInfo &map);
};

#endif