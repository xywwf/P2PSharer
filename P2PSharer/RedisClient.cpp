#include "stdafx.h"
#include "RedisClient.h"
#include <map>

acl::log g_cli_redislog;

#define REDIS_KEY_RESOURCE_HASH_LIST "HashResourceList"

CRedisClient::CRedisClient()
{
	m_redis = NULL;

	g_cli_redislog.open("redis.log");
}


CRedisClient::~CRedisClient()
{
}

void CRedisClient::Test()
{
	acl::string key = "C++��Ƶ����.avi|425632588965abe4";
	acl::string val = "45123";
	AddResourceToHashList(key, val);

	key = "425632588965abe4";
	val = "00-11-22-33-44-55";
	AddMACToResourceSet(key, val);
	
	key = "J++��Ƶ����.avi|ff88a2588965abe4";
	val = "3252";
	AddResourceToHashList(key, val);

	key = "����Ա.avi|dd21aa2588965abe4";
	val = "53432";
	AddResourceToHashList(key, val);
	
	key = "C++";
	std::map<acl::string, acl::string> mapRes;
	bool bFind = FindResource(key, mapRes);

	key = "425632588965abe4";
	val = "00-11-22-33-44-55";
	RemoveMACFromResourceSet(key, val);
}

//��ʼ��
bool CRedisClient::Init(const char *addr)
{
	acl::acl_cpp_init();

	m_addr = addr;

	if (NULL == m_redis)
		m_redis = new acl::redis_client(m_addr);

	if (m_redis)
	{
		g_cli_redislog.msg1("����Redis������[%s]�ɹ���", addr);
		return true;
	}

	g_cli_redislog.msg1("����Redis������[%s]ʧ�ܣ�", addr);
	return false;
}

//����Դ���ӵ�Hash�� key��HashResList��filed���ļ���|�ļ�MD5��Value���ļ���С
bool CRedisClient::AddResourceToHashList(acl::string &field, acl::string &value)
{
	acl::redis_hash redis(m_redis);

	int ret = 0;
	redis.clear();
 	if ((ret = redis.hsetnx(REDIS_KEY_RESOURCE_HASH_LIST, field, value)) < 0)
	{
		g_cli_redislog.error1("��Hash�������ļ���Ϣʧ�ܣ��ļ�����%s, err:%d", field, acl::last_error());
		return false;
	}

	return true;
}

//����Դ�б��в�����Դ�ļ�
//������keyword �ؼ���
//      mapResult �����ļ����а���key���ļ��б���Ϣ
bool CRedisClient::FindResource(acl::string &keyword, std::map<acl::string, acl::string> &mapResult)
{
	acl::redis_hash redis(m_redis);
	int cursor = 0;

	std::map <acl::string, acl::string> mapTemp;
	acl::string pattern;
	pattern.format("*%s*|*", keyword.c_str());   //�ļ����а���key������MD5

	do 
	{
		mapTemp.clear();
		cursor = redis.hscan(REDIS_KEY_RESOURCE_HASH_LIST, cursor, mapTemp, pattern);	
		if (cursor == -1)
		{
			g_cli_redislog.error1("������Դ�ļ�ʧ�ܣ�err:%d", acl::last_error());
			return false;
		}

		mapResult.insert(mapTemp.begin(), mapTemp.end());

	} while (cursor > 0);

	return true;
}

//����Դ�ļ��ĵ�ַ��������MAC����set��ʽ�洢��key:�ļ�MD5 members:MAC��ַ
bool CRedisClient::AddMACToResourceSet(acl::string &key, acl::string &mac)
{
	acl::redis_set redis(m_redis);
	std::vector<acl::string> members;
	members.push_back(mac);

 	if (-1 == redis.sadd(key, members))
	{
		g_cli_redislog.error1("���ļ�[%s]�ĵ�ַ��������MACʧ�ܣ�err:%d", key, acl::last_error());
		return false;
	}
	
	return true;
}

//����Դ�ļ��ĵ�ַ����ɾ��MAC���籾���ļ������
bool CRedisClient::RemoveMACFromResourceSet(acl::string &key, acl::string &mac)
{
	acl::redis_set redis(m_redis);
	std::vector<acl::string> members;
	members.push_back(mac);

	if (-1 == redis.srem(key, members))
	{
		g_cli_redislog.error1("���ļ�[%s]�ĵ�ַ����ɾ��MAC��ַʧ�ܣ�err:%d", key, acl::last_error());
		return false;
	}

	return true;
}