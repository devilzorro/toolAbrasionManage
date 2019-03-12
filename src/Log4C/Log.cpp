//
// Created by llj on 18-4-8.
//

#include "Log.h"

std::string str("selfLog");
//std::wstring wstr=str.c_str();
Logger  Log::_logger = log4cplus::Logger::getInstance(str);

std::string strFile1("LogToFile1");
//std::wstring wstr=str.c_str();
Logger  Log::_loggerF1 = log4cplus::Logger::getInstance(strFile1);

std::string strFile2("LogToFile2");
//std::wstring wstr=str.c_str();
Logger  Log::_loggerF2 = log4cplus::Logger::getInstance(strFile2);

Log::Log()
{

}

Log::~Log()
{
}

Log& Log::instance()
{
    static Log log;
    return log;
}

bool Log::open_log()
{
    log4cplus::PropertyConfigurator::doConfigure("log4cplus.properties");//将要读取的配置文件的路径
    return true;
}

/************************************************************************/
/* log的使用方法：
if (!Log::instance().open_log())
{
std::cout << "Log::open_log() failed" << std::endl;
}
INFOLOG("Server init succ");
ERRORLOG("Server run failed"); */
/************************************************************************/