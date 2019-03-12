//
// Created by llj on 18-4-8.
//

#ifndef SELF_LOG_H
#define SELF_LOG_H


#include "log4cplus/loglevel.h"
#include "log4cplus/ndc.h"
#include "log4cplus/logger.h"
#include "log4cplus/configurator.h"
#include "iomanip"
#include "log4cplus/fileappender.h"
#include "log4cplus/layout.h"
#include "log4cplus/loggingmacros.h"

using namespace log4cplus;
using namespace log4cplus::helpers;

//日志封装
#define TRACELOG(p) LOG4CPLUS_TRACE(Log::_logger, p)
#define DEBUGLOG(p) LOG4CPLUS_DEBUG(Log::_logger, p)
#define INFOLOG(p) LOG4CPLUS_INFO(Log::_logger, p)
#define WARNLOG(p) LOG4CPLUS_WARN(Log::_logger, p)
#define ERRORLOG(p) LOG4CPLUS_ERROR(Log::_logger, p)

#define INFOLOG_F1(p) LOG4CPLUS_INFO(Log::_loggerF1, p)
#define INFOLOG_F2(p) LOG4CPLUS_INFO(Log::_loggerF2, p)

// 日志控制类，全局共用一个日志
class Log
{
public:
    //打开日志
    bool open_log();

    //获得日志实例
    static Log & instance();

    //全局日志对象
    static Logger _logger;
    static Logger _loggerF1;
    static Logger _loggerF2;

    //
    //std::string str="DigitalImageProcess_log";

private:
    Log();

    virtual ~Log();


};
#endif //SELF_LOG_H
