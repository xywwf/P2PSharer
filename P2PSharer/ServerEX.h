#pragma once
#include "stdafx.h"
#include <iostream>
#include "lib_acl.h"
#include "acl_cpp\lib_acl.hpp"
#include "CommonDefine.h"
#include "PeerList.h"
#include "FlagMgr.h"
#include "FileClient.h"

class ServerEX: public acl::thread
{
public:
	ServerEX();
	~ServerEX();

	//初始化本地UDP并记录服务端地址
	bool Init(const char* server_addr);

	//接收数据的线程函数
	void* run();

	//返回socket，测试用？？？
	acl::socket_stream &GetSockStream();

	//获取本机的外网地址
	char *GetNatIP();

	//发送登录消息
	bool SendMsg_UserLogin();

	//请求服务端转发P2P打洞请求
	bool SendMsg_P2PConnect(const char *mac);

	//请求获取指定MAC的IP地址，以便进行打洞通信等操作
	bool SendMsg_GetIPofMAC(const char *mac);
	bool SendMsg_GetIPofMAC(const char *mac, acl::string &ip);

	//发送其它。。。

	//发送P2P数据，仅测试
	bool SendMsg_P2PData_BaseMAC(const char *data, const char *tomac);
	bool SendMsg_P2PData_BaseMAC(void *data, size_t size, const char *toMac);

	//向指定地址发送数据
	bool SendData(void *data, size_t size, acl::socket_stream *stream, const char *addr);

	//向指定地址发送数据
	bool SendMsgToServer(void *data, size_t size);

	//若连续两个心跳时间未收到服务器查询消息则重新登录
	void DoLogin();
private:
	//获取本机所有网卡IP
	void GetLocalIPs(Peer_Info &peerInfo, acl::string portInfo);

	//登录确认消息
	void ProcMsgUserLoginAck(MSGDef::TMSG_HEADER *data);

	//收到请求P2P连接（打洞）的消息
	void ProcMsgP2PConnect(MSGDef::TMSG_HEADER *data, acl::socket_stream *stream);

	//收到确认打洞成功的消息
	void ProcMsgP2PConnectAck(MSGDef::TMSG_HEADER *data, acl::socket_stream *stream);

	//服务方收到下载请求后，向客户发送数据
	void ProcMsgP2PData(MSGDef::TMSG_HEADER *data);

	//客户端收到数据后确认
	void ProcMsgP2PDataAck(MSGDef::TMSG_HEADER *data);

	//收到请求下载数据块的消息
	void ProcMsgGetBlocks(MSGDef::TMSG_HEADER *data, acl::socket_stream *stream);

	//收到请求下载数据块的消息
	void ProcMsgGetBlocks2(MSGDef::TMSG_HEADER *data, acl::socket_stream *stream);

	////服务方收到协商请求下载的文件
	//void ProcMsgReqFile(MSGDef::TMSG_HEADER *data);

	////协商请求下载的文件成功
	//void ProcMsgReqFileAck(MSGDef::TMSG_HEADER *data);

	//服务方收到客户方请求哪些块数据
	void ProcMsgGetBlocksAck(MSGDef::TMSG_HEADER *data);

	//收到查询是否存活消息
	void ProcMsgUserActiveQuery(MSGDef::TMSG_HEADER *data, acl::socket_stream *stream);

	//收到所请求指定MAC的IP
	void ProcMsgGetUserClientAck(MSGDef::TMSG_HEADER *data);

	//收到文件下载数据
	void ProcMsgFileBlockData(MSGDef::TMSG_HEADER *data);

	//收到文件下载数据(服务器转发过来的)
	void ProcMsgFileBlockData2(MSGDef::TMSG_HEADER *data);

	////向指定地址发送数据
	//bool SendData(void *data, size_t size, acl::socket_stream *stream, const char *addr);

	//根据IP获取MAC地址
	const char *GetMACFromIP(const char *ip);

	acl::string server_addr_;              //服务端地址
	Peer_Info m_peerInfo;                  //本机信息
	acl::string m_errmsg;                  //错误信息
	DWORD m_dwLastActiveTime;              //上次与服务器保持通信的时间

	acl::locker m_lockSockStream;
	acl::socket_stream m_sockstream;
	bool m_bExit;

	PeerList m_lstUser;                      //已与本机连接的客户端列表

	CFlagMgr *m_objFlagMgr;
	CFileClient *m_objReciever;                //文件接收对象
};

