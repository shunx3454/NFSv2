#include "rpc.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include "utils.h"

// 请求头解析
RPCHandle::RPCHandle(const char *data_n, int len)
{
	RpcHeadSize = 0;
	Xid = ntohl(memrw(data_n));
	MessageType = ntohl(memrw(data_n + 4));
	RpcVersion = ntohl(memrw(data_n + 8));
	ProgramNnum = ntohl(memrw(data_n + 12));
	PorgramVersion = ntohl(memrw(data_n + 16));
	ProcessNum = ntohl(memrw(data_n + 20));
	CredFlavor = ntohl(memrw(data_n + 24));
	CredLen = ntohl(memrw(data_n + 28));

	try
	{
		if (CredLen)
		{
			RpcHeadSize += CredLen;
			CredData.resize(CredLen);
			CredData.insert(CredData.begin(), CredLen, *(data_n + 32)); // 不解析Data，仅保留字节流
		}
		VerifFlavor = ntohl(memrw(data_n + 32 + CredLen));
		VerifLen = ntohl(memrw(data_n + 36 + CredLen));
		if (VerifLen)
		{
			RpcHeadSize += VerifLen;
			VerifData.resize(VerifLen);
			VerifData.insert(VerifData.begin(), VerifLen, *(data_n + 36 + CredLen)); // 不解析Data，仅保留字节流
		}
		RpcHeadSize += 40;
		RPCbytes.resize(RpcHeadSize);
		RPCbytes.insert(RPCbytes.begin(), RpcHeadSize, *data_n);
	}
	catch (std::exception &e)
	{
		std::cout << "RPC parse exception: " << e.what() << std::endl;
	}
}

// 请求头创建
/* RPCHandle::RPCHandle(int xid, int prog, int progver, int proc, int msgtype = 0, int rpcver = 2,
					 int credflv = 0, int credlen = 0, int verflv = 0, int verlen = 0, char *creddata = nullptr, char *verdata = nullptr)
{
	RpcHeadSize = 0;
	Xid = xid;
	MessageType = msgtype;
	RpcVersion = rpcver;
	ProgramNnum = prog;
	PorgramVersion = progver;
	ProcessNum = proc;
	CredFlavor = credflv;
	CredLen = credlen;
	if (CredLen)
	{
		RpcHeadSize += CredLen;
		CredData.resize(CredLen);
		CredData.insert(CredData.begin(), CredLen, *(creddata));
	}
	VerifFlavor = verflv;
	VerifLen = verlen;
	if (VerifLen)
	{
		RpcHeadSize += VerifLen;
		VerifData.resize(VerifLen);
		VerifData.insert(VerifData.begin(), VerifLen, *(verdata));
	}
	RpcHeadSize += 40;
	RPCbytes.resize(RpcHeadSize);
	memww(&(RPCbytes.at(0)), htonl(Xid));
	memww(&(RPCbytes.at(4)), htonl(MessageType));
	memww(&(RPCbytes.at(8)), htonl(RpcVersion));
	memww(&(RPCbytes.at(12)), htonl(ProgramNnum));
	memww(&(RPCbytes.at(16)), htonl(PorgramVersion));
	memww(&(RPCbytes.at(20)), htonl(ProcessNum));
	memww(&(RPCbytes.at(24)), htonl(CredFlavor));
	memww(&(RPCbytes.at(28)), htonl(CredLen));
	if (CredLen)
	{
		RPCbytes.insert(RPCbytes.begin() + 32, CredData.begin(), CredData.end());
	}
	memww(&(RPCbytes.at(32 + CredLen)), htonl(VerifFlavor));
	memww(&(RPCbytes.at(36 + CredLen)), htonl(VerifLen));
	if (VerifLen)
	{
		RPCbytes.insert(RPCbytes.begin() + 36, CredData.begin(), CredData.end());
	}
} */

// 响应头创建
/*
acceptstate：
0（SUCCESS）：请求成功。
1（PROG_UNAVAIL）：程序不可用。
2（PROG_MISMATCH）：程序版本不匹配。
3（PROC_UNAVAIL）：过程不可用。
5（AUTH_FAILED）：认证失败。
 */
RPCHandle::RPCHandle(int xid, int msgtype, int replystate, int acceptstate, int verflv, int verlen, char *verdata)
{
	RpcHeadSize = 0;
	Xid = xid;
	MessageType = msgtype;
	ReplyState = replystate;
	AcceptSate = acceptstate;
	VerifFlavor = verflv;
	VerifLen = verlen;

	if (VerifLen)
	{
		RpcHeadSize += VerifLen;
		VerifData.resize(VerifLen);
		VerifData.insert(VerifData.begin(), VerifLen, *verdata);
	}
	RpcHeadSize += 24;
	RPCbytes.resize(RpcHeadSize);
	memww(&(RPCbytes.at(0)), htonl(Xid));
	memww(&(RPCbytes.at(4)), htonl(MessageType));
	memww(&(RPCbytes.at(8)), htonl(ReplyState));
	memww(&(RPCbytes.at(12)), htonl(VerifFlavor));
	memww(&(RPCbytes.at(16)), htonl(VerifLen));
	RPCbytes.insert(RPCbytes.begin() + 20, VerifLen, *(verdata));
	memww(&(RPCbytes.at(20 + VerifLen)), htonl(AcceptSate));
}

void RPCHandle::print(int flag)
{
	if (flag == 0)
	{
		std::cout << std::endl
				  << "RPC Requst Head: xid=" << Xid
				  << " msg_type=" << MessageType
				  << " rpc_vers=" << RpcVersion
				  << " prog=" << ProgramNnum
				  << " prog_vers=" << PorgramVersion
				  << " proc=" << ProcessNum
				  << " credentail_flavor=" << CredFlavor
				  << " credentail_len=" << CredLen
				  << " verifier_flavor=" << VerifFlavor
				  << " verifier_len=" << VerifLen
				  << std::endl;
	}
	else if (flag == 1)
	{
		std::cout << std::endl
				  << "RPC Response Head: xid=" << Xid
				  << " msg_type=" << MessageType
				  << " replay_state=" << ReplyState
				  << " flavor=" << CredFlavor
				  << " len=" << CredLen
				  << " accept_state=" << AcceptSate
				  << std::endl;
	}
}