#pragma once

#include <vector>

class RPCHandle
{
public:
	int Xid;
	int MessageType;
	int RpcVersion;
	int ProgramNnum;
	int PorgramVersion;
	int ProcessNum;
	int CredFlavor;
	int CredLen;
	std::vector<char> CredData;
	int VerifFlavor;
	int VerifLen;
	int RpcHeadSize;
	int ReplyState;
	int AcceptSate;
	std::vector<char> VerifData; // 未解析，仅保留原始字节流
	std::vector<char> RPCbytes;	 // 未解析，仅保留原始字节流

	void print(int flag);
	RPCHandle(const char *data, int len); // 解析RPC请求头
	RPCHandle(int xid, int msgtype = 1, int replystate = 0, int acceptstate = 0,
			  int verflv = 0, int verlen = 0, char *verdata = nullptr); // 创建RPC响应头
	~RPCHandle() = default;
};

