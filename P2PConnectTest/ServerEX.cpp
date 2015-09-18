#include "stdafx.h"
#include "ServerEX.h"


static int __timeout = 10;

acl::log g_cli_exlog;

ServerEX::ServerEX()
{
	g_cli_exlog.open("serEx.log", "ServerEx");

	m_bExit = false;

	m_objFlagMgr = new CFlagMgr;
	
}


ServerEX::~ServerEX()
{
	m_bExit = true;
	
}

//��ʼ������UDP����¼����˵�ַ
bool ServerEX::Init(const char* addr, const char *name)
{
	if (NULL == m_objFlagMgr)
	{
		g_cli_exlog.fatal1("��Ƕ�����Ϊ�գ�");
		return false;
	}

	server_addr_ = addr;
	acl::acl_cpp_init();

	if (!m_sockstream.bind_udp("0.0.0.0:0"))
	{
		g_cli_exlog.fatal1("�󶨱���UDP�˿�ʧ�ܣ�%d", acl::last_error());
		return false;
	}

	g_cli_exlog.msg1("�󶨱���UDP�˿�:%s", m_sockstream.get_local(true));
// 	memcpy(m_peerInfo.szMAC, GetMacAddr().c_str(), MAX_MACADDR_LEN);
	memcpy(m_peerInfo.szMAC, name, MAX_MACADDR_LEN);

	//��ȡ��������IP��ַ
	acl::string tmpForPort = m_sockstream.get_local(true);
	char *portInfo = tmpForPort.find(":");
	GetLocalIPs(m_peerInfo, portInfo);
	m_sockstream.set_rw_timeout(0);

	return true;
}

//��ȡ������������IP
void ServerEX::GetLocalIPs(Peer_Info &peerInfo, acl::string portInfo)
{
	// �õ�������IP��ַ
	char szHost[256];
	::gethostname(szHost, 256);

	// �õ�����������������IP��ַ�Ͷ˿ں�,��Щ����˽�е�ַ/�˿ں�
	char *pIP;
	hostent* pHost = ::gethostbyname(szHost);
	for (int i = 0; i < MAX_ADDNUM - 1; ++i)
	{
		if (NULL == (pIP = pHost->h_addr_list[i]))
		{
			break;
		}
		struct in_addr tmpIp;
		memcpy(&tmpIp, pIP, pHost->h_length);
		_snprintf_s(peerInfo.arrAddr[i].IPAddr, MAX_ADDR_LENGTH, "%s%s", inet_ntoa(tmpIp), portInfo.c_str());

		++peerInfo.nAddrNum;
	}
}

void* ServerEX::run()
{
	char buf[1300];
	MSGDef::TMSG_HEADER *msg;

	while (!m_bExit)
	{
		memset(buf, 0, sizeof(buf));
		int iret = m_sockstream.read(buf, sizeof(buf), false);
		if (iret < 0)
		{
			g_cli_exlog.error1("��ȡ����ʧ�ܣ�%s", m_sockstream.get_peer(true));
			continue;
		}

		msg = (MSGDef::TMSG_HEADER*)buf;
		switch (msg->cMsgID)
		{
		case eMSG_USERLOGINACK:
			ProcMsgUserLoginAck(msg);
			break;
		case eMSG_P2PCONNECT:
			ProcMsgP2PConnect(msg, &m_sockstream);
			break;
		case eMSG_P2PCONNECTACK:
			ProcMsgP2PConnectAck(msg, &m_sockstream);
			break;
		case eMSG_P2PDATA:
			ProcMsgP2PData(msg);
			break;
		case eMSG_USERACTIVEQUERY:
			ProcMsgUserActiveQuery(msg, &m_sockstream);
			break;
		case eMSG_GETUSERCLIENTIPACK:
			ProcMsgGetUserClientAck(msg);
			break;
		default:
			g_cli_exlog.error1("�������Ϣ���ͣ�%d", msg->cMsgID);
			break;
		}
	}

	return NULL;
}

//��ָ����ַ��������
bool ServerEX::SendData(void *data, size_t size, acl::socket_stream *stream, const char *addr)
{
	m_lockSockStream.lock();

	if (stream->set_peer(addr))
	{
		if (stream->write(data, size) > 0)
		{
			m_lockSockStream.unlock();
			return true;
		}

		g_cli_exlog.error1("��[%s]��������ʧ��,err:%d", addr, acl::last_error());
	}
	else
    	g_cli_exlog.error1("����Զ�̵�ַ[%s]ʧ��,err:%d", addr, acl::last_error());

	m_lockSockStream.unlock();

	return false;
}


//���͵�¼��Ϣ
bool ServerEX::SendMsg_UserLogin()
{
	MSGDef::TMSG_USERLOGIN tMsgUserLogin(m_peerInfo);

	acl::string loginFlag = m_objFlagMgr->GetFlag(FORMAT_FLAG_LOGIN, server_addr_);
	m_objFlagMgr->SetFlag(loginFlag, 0);

	for (int iRetry = 0; iRetry < MAX_TRY_NUMBER; iRetry++)
	{
		if (SendData(&tMsgUserLogin, sizeof(tMsgUserLogin), &m_sockstream, server_addr_))
		{
			for (int iWait = 0; iWait < 20; iWait++)
			{
				if (m_objFlagMgr->WaitFlag(loginFlag))
				{
					g_cli_exlog.msg1("��¼�ɹ���");
					m_objFlagMgr->RMFlag(loginFlag);

					cout << "��¼�ɹ�������IP��" << m_peerInfo.arrAddr[m_peerInfo.nAddrNum - 1].IPAddr << endl;

					return true;
				}
				Sleep(100);
			}
		}

		Sleep(1000);
	}

	m_errmsg.format("��%s���͵�¼��Ϣʧ��,err:%d", m_sockstream.get_peer(true), acl::last_error());
	//MessageBox(NULL, m_errmsg, "client", MB_OK);
	cout << m_errmsg.c_str() << endl;
	return false;
}


//��¼ȷ����Ϣ
void ServerEX::ProcMsgUserLoginAck(MSGDef::TMSG_HEADER *data)
{
	MSGDef::TMSG_USERLOGINACK *msg = (MSGDef::TMSG_USERLOGINACK*)data;

	m_peerInfo = msg->PeerInfo;

	//���õ�¼�ɹ����
	m_objFlagMgr->SetFlag(m_objFlagMgr->GetFlag(FORMAT_FLAG_LOGIN, server_addr_), 1);

	m_errmsg.format("��¼�ɹ�������IP��%s", m_peerInfo.arrAddr[m_peerInfo.nAddrNum - 1].IPAddr);
	cout << m_errmsg.c_str() << endl;
	//MessageBox(NULL, m_errmsg, "client", MB_OK);
}

//��������ת��P2P������
//���迼��Ŀ��ͻ����������ߵ�������˿ڱ仯��!!!!!
bool ServerEX::SendMsg_P2PConnect(const char *mac)
{
	Peer_Info *peer = m_lstUser.GetAPeer(mac);
	if (NULL == peer)
	{
		if (!SendMsg_GetIPofMAC(mac))
		{
			cout << "��ȡIP��ַʧ�ܣ��û�����" << mac << endl;
			return false;
		}

		peer = m_lstUser.GetAPeer(mac);
		cout << "��ȡ��ַ�ɹ����û�����" << mac << endl;
	}

	m_errmsg.format("׼���������Ϣ��Ŀ�꣺%s", peer->arrAddr[peer->nAddrNum - 1].IPAddr);
	cout << m_errmsg.c_str() << endl;


	MSGDef::TMSG_P2PCONNECT tMsgConnect(m_peerInfo);
	memcpy(tMsgConnect.szMAC, mac, strlen(mac));

	//��ʼ�����ͱ��
	acl::string flag = m_objFlagMgr->GetFlag(FORMAT_FLAG_P2PCONN, peer->arrAddr[peer->nAddrNum - 1].IPAddr);
	m_objFlagMgr->SetFlag(flag, 0);

	//����Ŀ�귢�ʹ�
	for (int i = 0; i < 1; i++)
	{
		for (int i = 0; i < peer->nAddrNum; ++i)
		{
			SendData(&tMsgConnect, sizeof(tMsgConnect), &m_sockstream, peer->arrAddr[i].IPAddr);
		}
	}

	for (int iRetry = 0; iRetry < MAX_TRY_NUMBER; iRetry++)
	{
		if (SendData(&tMsgConnect, sizeof(tMsgConnect), &m_sockstream, server_addr_) && m_objFlagMgr->WaitFlag(flag))
		{
			m_errmsg.format("������[%s]����P2P��ת����Ϣ�ɹ�", server_addr_);
			m_objFlagMgr->RMFlag(flag);
			cout << m_errmsg.c_str() << endl;

			return true;
		}

		Sleep(1000);
	}

	m_errmsg.format("������[%s]����P2P��ת����Ϣʧ��,err:%d", m_sockstream.get_peer(true), acl::last_error());
	cout << m_errmsg.c_str() << endl;
	return false;
}

//�����ȡָ��MAC��IP��ַ���Ա���д�ͨ�ŵȲ���
bool ServerEX::SendMsg_GetIPofMAC(const char *mac)
{
	if (NULL != m_lstUser.GetAPeer(mac))
	{
		return true;
	}

	MSGDef::TMSG_GETUSERCLIENTIP tMsgGetClientIP(m_peerInfo);
			
	memcpy(tMsgGetClientIP.szMAC, mac, strlen(mac));

	//��ʼ�����ͱ��
	acl::string flag = m_objFlagMgr->GetFlag(FORMAT_FLAG_GETCLIENTIP, mac);
	m_objFlagMgr->SetFlag(flag, 0);	

	for (int iRetry = 0; iRetry < MAX_TRY_NUMBER; iRetry++)
	{
		if (SendData(&tMsgGetClientIP, sizeof(tMsgGetClientIP), &m_sockstream, server_addr_) && m_objFlagMgr->WaitFlag(flag))
		{
			g_cli_exlog.msg1("������[%s]��������[%s]��IP��ַ�ɹ�", server_addr_, mac);
			m_objFlagMgr->RMFlag(flag);


 			m_errmsg.format("��ȡ[%s]�Ŀͻ�����Ϣ�ɹ���", mac);
			cout << m_errmsg.c_str() << endl;

			return true;
		}

		Sleep(1000);
	}

	m_errmsg.format("������[%s]��������[%s]��IP��ַʧ��", server_addr_.c_str(), mac);
	cout << m_errmsg.c_str() << endl;
	return false;

}

//�����ȡָ��MAC��IP��ַ������
bool ServerEX::SendMsg_GetIPofMAC(const char *mac, acl::string &ip)
{
	if (SendMsg_GetIPofMAC(mac))
	{
		Peer_Info *peer = m_lstUser.GetAPeer(mac);
		ip = peer->arrAddr[peer->nAddrNum - 1].IPAddr;

		return true;
	}

	return false;
}

//�յ�ȷ�ϴ򶴳ɹ�����Ϣ
void ServerEX::ProcMsgP2PConnectAck(MSGDef::TMSG_HEADER *data, acl::socket_stream *stream)
{
	m_errmsg.format("----------�յ�%sȷ�ϴ򶴳ɹ�����Ϣ", stream->get_peer(true));
	cout << m_errmsg.c_str() << endl;

	MSGDef::TMSG_P2PCONNECTACK *msg = (MSGDef::TMSG_P2PCONNECTACK *)data;
	memcpy(msg->PeerInfo.P2PAddr, stream->get_peer(true), MAX_ADDR_LENGTH);

	//����P2P��ַ���ʱ����
	if (0 != strcmp(m_lstUser.GetAPeer(msg->PeerInfo.szMAC)->P2PAddr, msg->PeerInfo.P2PAddr))
	{
		//ɾ���ɵ�
		m_lstUser.DeleteAPeer(msg->PeerInfo.szMAC);

		//�򶴳ɹ��Ŀͻ��˴����б�
		m_lstUser.AddPeer(msg->PeerInfo);
	}

	//���ô򶴳ɹ����
	m_objFlagMgr->SetFlag(m_objFlagMgr->GetFlag(FORMAT_FLAG_P2PCONN, msg->PeerInfo.arrAddr[msg->PeerInfo.nAddrNum - 1].IPAddr), 1);
	cout << "�յ���ȷ����Ϣ" << endl;

}

//�յ�����P2P���ӣ��򶴣�����Ϣ
void ServerEX::ProcMsgP2PConnect(MSGDef::TMSG_HEADER *data, acl::socket_stream *stream)
{
	MSGDef::TMSG_P2PCONNECT *msg = (MSGDef::TMSG_P2PCONNECT*)data;
	m_errmsg.format("�յ�������Դ��%s ׼���ظ���\r\n", stream->get_peer(true));
	cout << m_errmsg.c_str() << endl;

	MSGDef::TMSG_P2PCONNECTACK tMsgP2PConnectAck(m_peerInfo);
	if (!strcmp(stream->get_peer(true), server_addr_))
	{
		//������ת�������ģ�����Դ���󷽵�����IP�ظ�
		for (int i = 0; i < msg->PeerInfo.nAddrNum; ++i)
		{
			acl::string ip = msg->PeerInfo.arrAddr[i].IPAddr;
			if (SendData(&tMsgP2PConnectAck, sizeof(tMsgP2PConnectAck), &m_sockstream, ip))
			{
				m_errmsg.format("��[%s]�ظ�ȷ�ϳɹ�", ip);
			}
		}
	}
	else
	{
	    //�ͻ��˷��͹����ģ���ֱ�ӻظ�
		Peer_Info *peer = m_lstUser.GetAPeer(msg->PeerInfo.szMAC);
		if (NULL != peer && strlen(peer->P2PAddr) <= 1)
		{
			memcpy(peer->P2PAddr, stream->get_peer(true), MAX_ADDR_LENGTH);
		}

		SendData(&tMsgP2PConnectAck, sizeof(tMsgP2PConnectAck), &m_sockstream, stream->get_peer(true));
	}
}


//����P2P���ݣ�������
bool ServerEX::SendMsg_P2PData(const char *data, const char *tomac)
{
	for (int iRetry = 0; iRetry < 10; iRetry++)
	{
		Peer_Info *peer = m_lstUser.GetAPeer(tomac);
		if (peer != NULL && strlen(peer->P2PAddr) > 1)
		{
			//���Է�������
			MSGDef::TMSG_P2PDATA tMsgP2PData(m_peerInfo);
			memcpy(tMsgP2PData.szMsg, data, strlen(data));
			if (SendData(&tMsgP2PData, sizeof(tMsgP2PData), &m_sockstream, peer->P2PAddr))
			{
				//g_cli_exlog.msg1("��%s�������ݳɹ���", peer->IPAddr);
				cout << "�������ݳɹ���Ŀ�꣺" << peer->P2PAddr << endl;
				return true;
			}


			cout << "�����Ѳ����ã��������´򶴣�" << endl;
			//ShowMsg("�����Ѳ����ã��������´򶴣�");

			memcpy(peer->P2PAddr, 0, MAX_ADDR_LENGTH);
		}


		//���δ���ӻ���ʧ����ʼ��
		SendMsg_P2PConnect(tomac);
		Sleep(300);
	}
}

bool ServerEX::SendMsg_P2PData(void *data, size_t size, const char *toMac)
{
	for (int iRetry = 0; iRetry < 3; iRetry++)
	{
		Peer_Info *peer = m_lstUser.GetAPeer(toMac);
		if (peer != NULL  && strlen(peer->P2PAddr) > 1)
		{
			//���Է�������
			for (int i = 0; i < 3; i++)
			{
				if (SendData(data, size, &m_sockstream, peer->P2PAddr))
				{
					g_cli_exlog.msg1("��%s  ip:%s�������ݳɹ���", toMac, peer->P2PAddr);
					return true;
				}

				Sleep(1000);
			}

			ShowMsg("�����Ѳ����ã��������´򶴣�");
		}

		//���δ���ӻ���ʧ����ʼ��
		if (SendMsg_P2PConnect(toMac))
			continue;;

		Sleep(500);
	}

	return false;
}

//�����յ������������ͻ���������
void ServerEX::ProcMsgP2PData(MSGDef::TMSG_HEADER *data)
{
	MSGDef::TMSG_P2PDATA *msg = (MSGDef::TMSG_P2PDATA *)data;
	cout << "++++++++++ �յ����ݣ�" << msg->szMsg << endl;
}

//�յ���ѯ�Ƿ�����Ϣ
void ServerEX::ProcMsgUserActiveQuery(MSGDef::TMSG_HEADER *data, acl::socket_stream *stream)
{
	MSGDef::TMSG_USERACTIVEQUERY tMsgUserActive(m_peerInfo);
	//ShowMsg("�յ��������Ϣ!");

	if (!SendData(&tMsgUserActive, sizeof(tMsgUserActive), stream, server_addr_))
	{
		ShowError("�ظ������Ϣʧ�ܣ�");
	}
}

acl::socket_stream &ServerEX::GetSockStream()
{
	return m_sockstream;
}

//�յ�������ָ��MAC��IP
void ServerEX::ProcMsgGetUserClientAck(MSGDef::TMSG_HEADER *data)
{
	MSGDef::TMSG_GETUSERCLIENTIPACK *msg = (MSGDef::TMSG_GETUSERCLIENTIPACK *)data;

	//�����û��б�
	m_lstUser.AddPeer(msg->PeerInfo);

	//��������ɹ����
	acl::string flag = m_objFlagMgr->GetFlag(FORMAT_FLAG_GETCLIENTIP, msg->PeerInfo.szMAC);
	m_objFlagMgr->SetFlag(flag, 1);

	m_errmsg.format("�յ�%s�Ŀͻ�����Ϣ��", msg->PeerInfo.szMAC);
	cout << m_errmsg.c_str() << endl;
}
/*
//�յ��ļ���������
void ServerEX::ProcMsgFileBlockData(MSGDef::TMSG_HEADER *data)
{
	MSGDef::TMSG_FILEBLOCKDATA *msg = (MSGDef::TMSG_FILEBLOCKDATA *)data;
	ShowMsg("�յ��������ݿ飬����д���ļ�");

	std::map<acl::string, CDownloader *>::iterator itTemp = g_mapFileDownloader.find(msg->info.md5);
	if (itTemp != g_mapFileDownloader.end())
	{
		itTemp->second->Recieve(msg);
	}
}

//�յ������������ݿ����Ϣ
void ServerEX::ProcMsgGetBlocks(MSGDef::TMSG_HEADER *data, acl::socket_stream *stream)
{
	MSGDef::TMSG_GETBLOCKS *msg = (MSGDef::TMSG_GETBLOCKS *)data;
	static void *buf = new char[EACH_BLOCK_SIZE];

	ShowMsg("�յ�GetBlocks�������ݷֿ���Ϣ");

start:

	std::map<acl::string, CFileServer *>::iterator itTmp = g_mapFileServer.find(msg->FileBlock.md5);
	if (itTmp != g_mapFileServer.end())
	{
		for (int i = 0; i < sizeof(msg->FileBlock.block) / sizeof(DWORD); i++)
		{
			DWORD dwPos = msg->FileBlock.block[i];
			if (dwPos == 0)
			{
				break;
			}

			int len = EACH_BLOCK_SIZE;
			memset(buf, 0, EACH_BLOCK_SIZE);
			if (itTmp->second->GetBlockData(dwPos, buf, len))
			{
				ShowMsg("��ȡ�ֿ�ɹ���������ʼ���䣡");

				MSGDef::TMSG_FILEBLOCKDATA tdata;
				memcpy(tdata.info.md5, msg->FileBlock.md5, 32);
				tdata.info.dwBlockNumber = dwPos;
				tdata.info.datalen = len;
				memcpy(tdata.info.data, buf, EACH_BLOCK_SIZE);

				SendData(&tdata, sizeof(tdata), stream, stream->get_peer());
			}
			else
			{
				ShowMsg("��ȡ�ֿ�ʧ�ܣ�");
			}
		}
	}
	else
	{
		acl::string fullPath;
		fullPath.url_decode(g_resourceMgr->GetFileFullPath(msg->FileBlock.md5));
		
		m_errmsg.format("�ļ�����·��Ϊ��%s", fullPath.c_str());
		ShowMsg(m_errmsg);

		CFileServer *pFileServer = new CFileServer;
		if (!pFileServer->Init(fullPath, EACH_BLOCK_SIZE))
		{
			m_errmsg.format("��ʼ�������ļ�[%s]ʧ��!", fullPath.c_str());
			g_clientlog.error1(m_errmsg);

			ShowError(m_errmsg);
			return;
		}

		g_mapFileServer[msg->FileBlock.md5] = pFileServer;
		goto start;
	}
}
*/