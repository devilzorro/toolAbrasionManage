#include "redisClient.h"

CRedisClient::CRedisClient()
{
	m_connect = NULL;
	m_reply = NULL;
}

CRedisClient::~CRedisClient()
{
	if (m_connect)
	{
		Disconnect();
	}
	
	if (m_reply)
	{
		FreeReply();
	}   
}

bool CRedisClient::Connect(const std::string &host, int port)
{
	m_connect = redisConnect(host.c_str(), port);
	
	if (!m_connect)
	{
		printf("redis connect error: m_connect is null\n");
		return false;
	}
	
	if(m_connect->err)
	{
		printf("redis connect error: %s\n", m_connect->errstr);
		FreeConnect();
		return false;
	}
	
	printf("redis connect success\n");
	return true;
}

bool CRedisClient::Connect(const std::string &host, int port, int timeout)
{
	struct timeval tv;
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;;
	m_connect = redisConnectWithTimeout(host.c_str(), port, tv);
	
	if (!m_connect)
	{
		printf("redis connect error: m_connect is null\n");
		return false;
	}
	
	if(m_connect->err)
	{
		printf("redis connect error: %s\n", m_connect->errstr);
		FreeConnect();
		return false;
	}
	
	printf("redis connect success\n");
	return true;
}

void CRedisClient::Disconnect()
{
	FreeConnect();
	printf("redis disconnect success\n");
}

bool CRedisClient::Set(const std::string &key, const std::string &value)
{
	if (!m_connect)
	{
		return false;
	}

	m_reply = (redisReply*)redisCommand(m_connect, "SET %s %s", (char *)key.c_str(), (char *)value.c_str());
	
	if (!m_reply)
	{
		printf("redis connect error");
		FreeConnect();
		return false;
	}

#ifdef WIN32
	if ((m_reply->type != REDIS_REPLY_STATUS) || (_stricmp(m_reply->str,"OK")))
	{
		printf("redis set error, key = %s, value = %s\n", key.c_str(), value.c_str());
		FreeReply();
		return false;
	}
	else
	{
		printf("redis set success, key = %s, value = %s\n", key.c_str(), value.c_str());
		FreeReply();
		return true;
	}
#else
    if ((m_reply->type != REDIS_REPLY_STATUS) || (strcasecmp(m_reply->str,"OK")))
	{
		printf("redis set error, key = %s, value = %s\n", key.c_str(), value.c_str());
		FreeReply();
		return false;
	}
	else
	{
		printf("redis set success, key = %s, value = %s\n", key.c_str(), value.c_str());
		FreeReply();
		return true;
	}
#endif
}


bool CRedisClient::Get(const std::string &key, std::string &value)
{
	if (!m_connect)
	{
		return false;
	}

	m_reply = (redisReply*)redisCommand(m_connect, "GET %s", key.c_str());
	
	if (!m_reply)
	{
		printf("redis connect error");
		FreeConnect();
		return false;
	}
	
	if (m_reply->type != REDIS_REPLY_STRING)
	{
		printf("redis get key[%s] error, %s\n", key.c_str(), m_reply->str);
		FreeReply();
		return false;
	}
	else
	{
		std::string str = m_reply->str;
		printf("redis get key[%s] success, value = %s\n", key.c_str(), str.c_str());
		value = str;
		FreeReply();
		return true;
	}
}

bool CRedisClient::HGet(const std::string &key, const std::string &hkey, std::string &value)
{
	if (!m_connect)
	{
		return false;
	}

	m_reply = (redisReply*)redisCommand(m_connect, "HGET %s %s", key.c_str(), hkey.c_str());
	
	if (!m_reply)
	{
		printf("redis connect error");
		FreeConnect();
		return false;
	}
	
	if (m_reply->type != REDIS_REPLY_STRING)
	{
		printf("redis hget key[%s] hkey[%s] error, %s\n", key.c_str(), hkey.c_str(), m_reply->str);
		FreeReply();
		return false;
	}
	else
	{
		std::string str = m_reply->str;
		printf("redis hget key[%s] hkey[%s] success, value = %s\n", key.c_str(), hkey.c_str(), str.c_str());
		value = str;
		FreeReply();
		return true;
	}
}

bool CRedisClient::CheckStatus()
{
	if (!m_connect)
	{
		printf("redis check status: connect is null\n");
		return false;
	}
	
    m_reply = (redisReply*)redisCommand(m_connect, "ping");
    if(!m_reply)
	{
		printf("redis check status: reply is null\n");
		FreeConnect();
		return false;
	}
	
    if (m_reply->type != REDIS_REPLY_STATUS)
	{
		printf("redis check status: reply type error\n");
		FreeReply();
		FreeConnect();
		return false;
	}
#ifdef WIN32
    if (_stricmp(m_reply->str,"PONG"))
    {
        printf("redis check status: reply not pong\n");
        FreeReply();
        FreeConnect();
        return false;
    }
#else
        if (strcasecmp(m_reply->str,"PONG"))
	{
		printf("redis check status: reply not pong\n");
		FreeReply();
		FreeConnect();
		return false;
	}
#endif

	else
	{
		printf("redis check status: connect ok\n");
		FreeReply();
		return true;
	}
}

void CRedisClient::FreeConnect()
{
	if (m_connect)
	{
		redisFree(m_connect);
		m_connect = NULL;
	}
}

void CRedisClient::FreeReply()
{
	if (m_reply)
	{
		freeReplyObject(m_reply);
		m_reply = NULL;
	}
}