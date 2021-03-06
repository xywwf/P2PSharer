/////////////////////////////////////
//分块请求下载类：控制向服务方发送下载请求
/////////////////////////////////////

#pragma once
#include "acl_cpp\stdlib\thread.hpp"
#include "CommonDefine.h"

class CDownloader;

class CReqSender :
	public acl::thread
{
public:
	CReqSender();
	~CReqSender();

	//初始化
	bool Init(const char *toaddr, acl::socket_stream &sock, acl::string &md5, CDownloader *pNotify);

	//分配一批数据块序号给本对象下载,非空时失败返回false
	bool PushTask(std::vector<DWORD> &blockNums);

	virtual void *run();

	//停止
	void Stop();

private:
	//拼装要请求下载数据块的数据包
	bool MakeRequestHeader(MSGDef::TMSG_GETBLOCKS &msg);
	bool MakeRequestHeader2(MSGDef::TMSG_GETBLOCKS2 &msg);

	//发送请求 可指定是否需要服务器转发
	bool SendRequest(void *data, size_t size, bool bTransmit);

	//直接P2P
	int P2PNoTrasmit();
	int P2PWithTransmit();

	acl::locker m_lockBlockNum;
	std::vector<DWORD>  m_vBlockNums;        //需要下载的数据块序号
	acl::string m_macAddr;                   //目标IP地址
	acl::string m_fileMD5;                   //与该类对象绑定的文件MD5标记
	bool m_bExit;

	CDownloader *m_pNotify;                 //CDownloader对象指针
	//acl::socket_stream *m_sock;              //先共享用同一个SOCKET，调通后再试独立的SOCKET？？？？？？？？？？？？？？？
};

