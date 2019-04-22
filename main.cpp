#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <cctype>
#include <thread>
#include <chrono>
#include <atomic>
#include <fstream>
#include <sstream>
#include "json/json.h"
#include "redisClient/redisClient.h"
#include "config/config.h"
#include "Log4C/Log.h"
#include <vector>
#include <time.h>
#include <map>
#include "fileCtl/FileCtl.h"

#ifdef WIN32
#include "mqtt-win/async_client.h"
#include <Windows.h>
#include "win_stdafx/stdafx.h"
#include <objbase.h>
#pragma comment(lib,"ws2_32.lib")
#else
#include <mqtt/async_client.h>
#include <dlfcn.h>
#include <uuid/uuid.h>
#endif

struct Results {
    double *r;
    int n;
};

#define TOOLCONFIG "toolLife.json"
#define COMMCONFIG "test.ini"
#define LOCALCONFIG "localConfig.json"
#define SUBTOPIC "toolLife"
#define PUBTOPIC "remoteGroup"

using namespace std;

string machineId = "";
//学习状态
string studyStatus = "";
//学习id
string studyId = "";
//进入计算特征值状态
//string processStatus = "";

//配置文件中程序名
string programName = "";

//机头号
string configMachineNo = "";

string localTopic  = "Command/x/";

string strUploadUrl = "";
string strUploadToken = "";
string strBizId = "";
FileCtl fileCtl;
string resultToolVal = "";

long maxSize = 5*1024*1024;

//string programStartPoint = "";
//string programEndPoint = "";

#ifdef WIN32
string dllPath = "tooldll.dll";
#else
string dllPath = "./libtool.so";
#endif
string redisAddr = "127.0.0.1";
int redisPort = 6379;
string mqttAddr = "127.0.0.1";
string strMqttPort = "1883";

vector<string> vcRecvMsgs;
vector<string> vcSendMsgs;
vector<string> vcSendLocalMsgs;
vector<string> vcSendLocalUploadMsgs;

string strHhKeyVal = "2010";
string strHlKeyVal = "2011";

string strprocessStatus = "";

map<string,string> HhKeysMap;
map<string,string> HlKeysMap;
map<string,string> machineStatusMap;
map<string,string> addrMap;
map<string,string> readAddrMap;
map<string,string> recvUploadTokenMsgMap;

map<int,double> sumMap;
map<int,long> countMap;
map<int,double> resultMap;


map<string,string> redisMap;

map<int,double> maxLimMap;
map<int,double> maxSensMap;
map<int,double> minLimMap;
map<int,double> minSensMap;
vector<int> vcNoAlarmTool;
//map<int,double> alertMap;
Json::Value jAddrVal;

CRedisClient redisClient;
CRedisClient redisWriteClient;

typedef int (*pInitFun)();
typedef int (*pFeedFun)(char *jsonData);
typedef char* (*pResultFun)();

pInitFun initFun;
pFeedFun feedFun;
pResultFun resultFun;

class callback : public virtual mqtt::callback {
public:

    /**
	 * This method is called when the client is connected.
	 * @param cause
	 */
	bool bConnStatus = false;

    virtual void connected(const string &cause) {
        cout << "callback->connected..." << cause << endl;
        DEBUGLOG("callback->connected..." << cause);
        bConnStatus = true;
        int ret;
    }

    /**
     * This method is called when the connection to the server is lost.
     * @param cause
     */
    virtual void connection_lost(const string &cause) {
        cout << "callback->connection_lost..." << cause << endl;
        DEBUGLOG("callback->connection_lost..." << cause);
//        DEBUGLOG(cause);
//        mqtt_connect_flag = false;
        //...
        bConnStatus = false;
    }

    void delivery_complete(mqtt::delivery_token_ptr tok) override {
        cout << "\tDelivery complete for token: "
             << (tok ? tok->get_message_id() : -1) << endl;
    }

    void message_arrived(mqtt::const_message_ptr msg) override {
//        std::cout << "Message arrived :" << std::endl;
//        std::cout << "\ttopic: '" << msg->get_topic() << "'" << std::endl;
        std::cout << "\tpayload: '" << msg->to_string() << "'\n" << std::endl;
//        DEBUGLOG("recv msg topic:"<<msg->get_topic());
//        DEBUGLOG("payload:"<<msg->to_string());
        vcRecvMsgs.push_back(msg->to_string());
    }
};

string getCurrentTime() {
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm* ptm = localtime(&tt);
    char date[60] = {0};
    sprintf(date, "%d-%02d-%02d %02d:%02d:%02d",
    (int)ptm->tm_year + 1900,(int)ptm->tm_mon + 1,(int)ptm->tm_mday,
    (int)ptm->tm_hour,(int)ptm->tm_min,(int)ptm->tm_sec);
    return string(date);

}

string getSysTime(){ //13位秒级系统时间
    //format: [s : ms]
    struct timeval tv;
    gettimeofday(&tv,NULL);
    char s[128] = "";
    sprintf(s,"%ld%ld",tv.tv_sec,(tv.tv_usec/1000));
    string str = s;
    cout<<str.length()<<endl;
    return str;
}

vector<string> split(string strContent,string mark) {
    string::size_type pos;
    vector<string> result;
    strContent += mark;
    int size = strContent.size();
    for(int i=0; i<size; i++)
    {
        pos = strContent.find(mark,i);
        if(pos<size)
        {
            string s = strContent.substr(i,pos-i);
            result.push_back(s);
            i = pos+mark.size()-1;
        }
    }
    return result;
}


string GetGuid()
{
    char szuuid[128] = { 0 };
#ifdef WIN32
    GUID guid;
    CoCreateGuid(&guid);
    _snprintf_s(
            szuuid,
            sizeof(szuuid),
            "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
            guid.Data1, guid.Data2, guid.Data3,
            guid.Data4[0], guid.Data4[1],
            guid.Data4[2], guid.Data4[3],
            guid.Data4[4], guid.Data4[5],
            guid.Data4[6], guid.Data4[7]);
#else
    uuid_t uuid;
	uuid_generate(uuid);
	uuid_unparse(uuid, szuuid);
#endif

    return string(szuuid);
}

//采集机床倍率信息 倍率异常生成报文 预警
void collectRateProcess() {
    while (1) {
        this_thread::sleep_for(chrono::seconds(20));

    }
}

string generMsg228(string uploadFileName) {
    Json::Value root;
    Json::Value contentRoot;
    Json::Value dataRoot;

    dataRoot["time"] = getCurrentTime();
    dataRoot["fileName"] = uploadFileName;

    contentRoot["data"] = dataRoot.toStyledString();
    contentRoot["dest"] = "toolLife";
    contentRoot["order"] = 228;
    contentRoot["switchs"] = "on";
    contentRoot["frequency"] = 1;
    contentRoot["source"] = "";
    contentRoot["cmdId"] = "";
    contentRoot["level"] = 7;
    contentRoot["concurrent"] = "true";

    root["content"] = contentRoot.toStyledString();
    root["encode"] = false;
    root["id"] = "";
    root["machineNo"] = machineId;
    root["type"] = 20;
    root["order"] = 228;
    root["dest"] = "toolLife";

    return root.toStyledString();
}

string generUploadLocalMsg(string fileName,string toolValRes) {
    Json::Value root;

    root["filePath"] = fileName;
    root["uploadToken"] = strUploadToken;
    root["uploadUrl"] = strUploadUrl;
    root["bizId"] = strBizId;
    root["bizData"] = toolValRes;

    return root.toStyledString();
}

void collectDataProcess() {
    cout<<"collect data thread running"<<endl;
    DEBUGLOG("collect data thread running");
    string tmpProgramName;
    while (1) {
        this_thread::sleep_for(chrono::milliseconds(10));
        //操作
        if (redisClient.CheckStatus()) {
            string hKey = "lastMessage_" + machineId;
            string str2010 = "";
            string str2011 = "";

            if (redisClient.HGet(hKey,strHhKeyVal,str2010)) {
                Json::Reader reader10;
                Json::Value root;

                if (reader10.parse(str2010,root)) {
                    string strRootVal = root["val"].asString();
                    Json::Reader tmpReader;
                    Json::Value rootVal;
                    if (tmpReader.parse(strRootVal,rootVal)) {
                        map<string,string>::iterator it;
                        for (it=HhKeysMap.begin();it!=HhKeysMap.end();++it) {
                            redisMap[it->first] = rootVal[it->second].asString();
//                            cout<<it->second<<" val "<<rootVal[it->second].asString()<<endl;
                        }
                    }

//                    for (int i = 0; i < vcHhKeysList.size(); ++i) {
//                        redisMap["load"] = root[vcHhKeysList[i]].asString();
//                    }

                }
            }

            if (redisClient.HGet(hKey,strHlKeyVal,str2011)) {
                Json::Reader reader11;
                Json::Value root;

                if (reader11.parse(str2011,root)) {
                    string strRootVal = root["val"].asString();
                    Json::Reader tmpReader;
                    Json::Value rootVal;
                    if (tmpReader.parse(strRootVal,rootVal)) {
                        map<string,string>::iterator it;
                        for (it=HlKeysMap.begin();it!=HlKeysMap.end();++it) {
                            redisMap[it->first] = rootVal[it->second].asString();
//                        cout<<it->second<<" val "<<root[it->second].asString()<<endl;
                        }
                    }
                }
            }
        }
    }
}

void programNameAlarm(string currentName) {
    if ((currentName != "")&&(programName != "")) {
        if (currentName != programName) {
            Json::Value root;
            Json::Value contentRoot;
            Json::Value dataRoot;

            dataRoot["time"] = getCurrentTime();
            dataRoot["errorId"] = GetGuid();
            dataRoot["detail"] = "程序名变更为:"+redisMap["programName"];

            contentRoot["data"] = dataRoot.toStyledString();
            contentRoot["dest"] = "toolLife";
            contentRoot["order"] = 227;
            contentRoot["switchs"] = "on";
            contentRoot["frequency"] = 1;
            contentRoot["source"] = "";
            contentRoot["cmdId"] = "";
            contentRoot["level"] = 7;
            contentRoot["concurrent"] = "true";

            root["content"] = contentRoot.toStyledString();
            root["encode"] = false;
            root["machineNo"] = machineId;
            root["type"] = 20;
            root["order"] = 227;
            root["dest"] = "toolLife";

            vcSendMsgs.push_back(root.toStyledString());
        }
    }
}


string processToolVal(string flag) {
    if (flag == "study") {
        cout<<"start study val count process"<<endl;
    } else {
        cout<<"start "<<flag<<" val count process"<<endl;
    }

    while (redisMap.empty()) {
        this_thread::sleep_for(chrono::milliseconds(500));
        cout<<"redisMap is empty"<<endl;
    }

    string tmpStartPoint = "";
    string tmpEndPoint = "";

    string tmpMachineStatus = "";
    string tmpEndStatus = "";

    if (redisMap.count("programStartTime")&&redisMap.count("programEndTime")) {
        tmpStartPoint = redisMap["programStartTime"];
        tmpEndPoint = redisMap["programEndTime"];
    }

    if (redisMap.count("mahineStatusKey")) {
        tmpMachineStatus = redisMap["mahineStatusKey"];
    }

    //程序名变更报警报文
    if (flag == "alarmProcess") {
        programNameAlarm(redisMap["programName"]);
    }

    cout<<"start point val:"<<tmpStartPoint<<endl;
    cout<<"end point val:"<<tmpEndPoint<<endl;
    DEBUGLOG("start point val:"<<tmpStartPoint);
    DEBUGLOG("end point val:"<<tmpEndPoint);
    cout<<"redis toolNo:"<<redisMap["toolNo"]<<endl;
    cout<<"redis load:"<<redisMap["load"]<<endl;
    DEBUGLOG("redis toolNo:"<<redisMap["toolNo"]);
    DEBUGLOG("redis load:"<<redisMap["load"]);

    double *tmpPointer = new double;
    double *tmpR;
    Results re;
    if(initFun() == 0);

    long tmpCount = 0;
    double tmpSum = 0;

    string currentStartPoint = "";
    long tmpCountStart = 0;

    string uploadTime = getSysTime();
    if (uploadTime.length() < 13) {
        uploadTime = uploadTime + "0";
    }
    string msg228 = generMsg228(redisMap["programName"]);
    vcSendMsgs.push_back(msg228);

    fileCtl.createNewzipFolder(tmpStartPoint);
    ofstream fWrite;
    long fileLength = 0;
    int fileSplite = 1;
    string destZipName = "/tmp/toolLife/" + uploadTime + "_1.zip";
    string uploadZipName;
    string uploadEOFName;
//    string uploadZipName = "/home/i5/data/file/" + tmpStartPoint + ".zip";
    while (1) {
        this_thread::sleep_for(chrono::milliseconds(10));
        if (!redisMap.empty()) {
            if (redisMap["countStatus"] == "false") {
//                cout<<"redis jobCountByStatus:"<<redisMap["countStatus"]<<endl;
                if (tmpStartPoint != "") {
//                    cout<<"redisMap time:"<<redisMap["programStartTime"]<<endl;
                    if (redisMap["programStartTime"] != tmpStartPoint) {
//                        cout<<"satrt counting.........."<<endl;


                        if (flag == "study") {
                            if (studyStatus == "abort") {
                                fileCtl.deleteFile(destZipName);
                                string strSplit;
                                stringstream ssSplit;
                                ssSplit<<fileSplite;
                                ssSplit>>strSplit;
                                string tmpFileName = "/tmp/toolLife/" + tmpStartPoint + "_" + strSplit;
                                fileCtl.deleteFile(tmpFileName);
//                                fWrite.close();
                                break;
                            }
                        }

                        //获取redis中存储的数据
//                        cout<<"redis toolNo:"<<redisMap["toolNo"]<<endl;
//                        cout<<"redis load:"<<redisMap["load"]<<endl;
                        if (redisMap.count("toolNo")&&redisMap.count("load")) {
                            tmpCount++;
                            stringstream ssToolNo;
                            stringstream ssLoad;
                            stringstream ssRpm;
                            int toolNo;
                            int iRpm;
                            double tmpload;
                            double load;
                            double dbTime;
                            ssToolNo<<redisMap["toolNo"];
                            ssToolNo>>toolNo;
                            ssLoad<<redisMap["load"];
                            ssLoad>>tmpload;
                            ssRpm<<redisMap["rpmKey"];
                            ssRpm>>iRpm;

                            string strTime = "";
                            if (redisMap["timeStamp"] != "") {
                                vector<string> tmpVcSplite = split(redisMap["timeStamp"],":");
                                strTime = tmpVcSplite[0] + "." + tmpVcSplite[1];
                                stringstream ssTime;
                                ssTime<<strTime;
                                ssTime>>dbTime;
                            }
//                            cout<<"toolNo"<<toolNo<<endl;
//                            cout<<"load:"<<tmpload<<endl;

                            Json::Value feedRoot;
                            Json::Value feedArrayRoot;
                            Json::Value arrayObj;
                            arrayObj["toolnum"] = toolNo;
                            arrayObj["load"] = tmpload;
                            arrayObj["time"] = dbTime;
                            arrayObj["rpm"] = iRpm;
                            feedArrayRoot.append(arrayObj);
                            feedRoot["feeddata"] = feedArrayRoot;
                            string strFeedData = feedRoot.toStyledString();
//                            cout<<"input feed data:"<<strFeedData<<endl;
                            feedFun(strFeedData.c_str());
                            tmpSum = tmpSum + tmpload;
//                            if (fWrite.is_open()) {
//                                string tmpWriteData = redisMap["toolNo"] + ":" + redisMap["load"] + ":"  + redisMap["rpmKey"] + ":" + strTime + "\n";
//                                fWrite<<tmpWriteData;
//                            }
                            if (fileLength <= maxSize) {
                                stringstream ssFileSplit;
                                string strSplit;
                                ssFileSplit<<fileSplite;
                                ssFileSplit>>strSplit;
                                string tmpFileName = "/tmp/toolLife/" + uploadTime + "_" + strSplit + ".txt";
                                fWrite.open(tmpFileName,ios::app);
                                string tmpWriteData = redisMap["toolNo"] + ":" + redisMap["load"] + ":"  + redisMap["rpmKey"] + ":" + strTime + "\n";
                                if (fWrite.is_open()) {
                                    fWrite<<tmpWriteData;
                                }
                                fWrite.close();
                                FILE *checkSize = fopen(tmpFileName.c_str(),"rb");
                                if (checkSize != NULL) {
                                    fseek(checkSize, 0, SEEK_END);
                                    fileLength = ftell(checkSize);
                                    rewind(checkSize);
                                }
                                fclose(checkSize);
                            } else {
                                string strSplit;
                                stringstream ssSplite;
                                ssSplite<<fileSplite;
                                ssSplite>>strSplit;
                                string zipSrcName = "/tmp/toolLife/" + uploadTime + "_" + strSplit + ".txt";
                                fileCtl.zipFile(zipSrcName,destZipName);
                                fileCtl.deleteFile(zipSrcName);
                                fileSplite++;
                                fileLength = 0;
                            }

                        }

                        if (tmpCountStart == 0) {
                            currentStartPoint = redisMap["programStartTime"];
                            tmpEndPoint = redisMap["programEndTime"];
                            cout<<"****************current startPoint:"<<redisMap["programStartTime"];
                        }
                        tmpCountStart++;
                        if ((currentStartPoint != "")&&(currentStartPoint != redisMap["programStartTime"])) {
                            cout<<"error start count end"<<endl;
//                            DEBUGLOG("error start count end");
                            strprocessStatus = "error end";
                            fileCtl.deleteFile(destZipName);
                            string strSplit;
                            stringstream ssSplit;
                            ssSplit<<fileSplite;
                            ssSplit>>strSplit;
                            string tmpFileName = "/tmp/toolLife/" + uploadTime + "_" + strSplit + ".txt";
                            fileCtl.deleteFile(tmpFileName);
//                            fWrite.close();
                            break;
                        }

                        if (redisMap["programEndTime"] != tmpEndPoint) {
                            cout<<"******count process end**********"<<endl;
                            cout<<"********************************"<<endl;
//                            DEBUGLOG("count process end");
                            cout<<"END***startPointVal:"<<redisMap["programStartTime"]<<endl;
                            cout<<"END***endPointVal:"<<redisMap["programEndTime"]<<endl;
//                            DEBUGLOG("startPointVal:"<<redisMap["programStartTime"]);
//                            DEBUGLOG("endPointVa:"<<redisMap["programEndTime"]);
//                            fWrite.close();
                            stringstream ssSplit;
                            string strSplit;
                            ssSplit<<fileSplite;
                            ssSplit>>strSplit;
                            string tmpOriginName = "/tmp/toolLife/" + uploadTime + "_" + strSplit +".txt";
                            fileCtl.zipFile(tmpOriginName,destZipName);
                            fileCtl.deleteFile(tmpOriginName);
                            uploadZipName = "/home/i5/data/file/" + strBizId + "_" + uploadTime + "_1.zip";
                            fileCtl.mvFile(destZipName,uploadZipName);
                            fileCtl.deleteFile(destZipName);
                            uploadEOFName = "/home/i5/data/file/" + strBizId + "_" + uploadTime +"_2_eof.txt";
                            ofstream eofWrite;
                            eofWrite.open(uploadEOFName,ios::app);
                            eofWrite.close();
                            //pub local mqtt uploadMsg
                            break;
                        }
                    }
                }
            } else if(redisMap["countStatus"] == "true") {
                if (redisMap["mahineStatusKey"] == machineStatusMap["hold"]) {
                    if (tmpMachineStatus == machineStatusMap["free"]) {
                        if (redisMap["mahineStatusKey"] == machineStatusMap["work"]) {
                            if (redisMap.count("toolNo")&&redisMap.count("load")) {
                                stringstream ss;
                                int toolNo;
                                double tmpload;
                                ss<<redisMap["toolNo"];
                                ss>>toolNo;
                                ss<<redisMap["load"];
                                ss>>tmpload;
                                char *tmpData = "";
                                feedFun(tmpData);
                            }
                        } else if (redisMap["mahineStatusKey"] == machineStatusMap["free"]) {
                            break;
                        }
                    } else if (tmpMachineStatus == machineStatusMap["work"]) {
                        break;
                    }
                }
            } else {

            }

        }

    }

    cout<<"count val process end"<<endl;
    DEBUGLOG("count val process end");
//    double tmpDresult = tmpSum/tmpCount;
//    cout<<"********test result:"<<tmpDresult<<endl;

//    char chData[5000];
//    memset(chData,'\0',5000);
    char *chData = resultFun();

    string strRet = chData;
    Json::Value retRoot;
    retRoot["toolVal"] = strRet;
    retRoot["exeName"] = redisMap["programName"];
    string strJsonRet = retRoot.toStyledString();
    string uploadZipMsg = generUploadLocalMsg(uploadZipName,strJsonRet);
    vcSendLocalUploadMsgs.push_back(uploadZipMsg);

    string uploadEofMsg = generUploadLocalMsg(uploadEOFName,strJsonRet);
    vcSendLocalUploadMsgs.push_back(uploadEofMsg);
//    DEBUGLOG("count process result:"<<chData);
    delete tmpPointer;
//    tmpPointer = NULL;
    char chCopy[5000];
    memset(chCopy,'\0',5000);
    strcpy(chCopy,chData);
    resultToolVal = chCopy;
    cout<<"resultToolVal:"<<chData<<endl;
    return chData;
}

void studyProcess(string content,string studyId) {
//    string studyRe = processToolVal(mode);
    string studyRe = content;
    if (studyStatus == "abort") {
        cout<<"study process abort"<<endl;
        DEBUGLOG("study process abort");
    } else {
        //生成阈值报文回复
        Json::Value root;
        Json::Value contentRoot;
        Json::Value dataRoot;
        Json::Value studyResult;

//        double *r;
//        r = studyRe.r;
//        for (int i = 0; i < studyRe.n; ++i) {
//            int toolNo = r[i*2];
//            double val = r[i*2]+1;
//            Json::Value studyResultContent;
//            studyResultContent["toolNo"] = toolNo;
//            studyResultContent["value"] = val;
//            studyResult.append(studyResultContent);
//        }
//        delete r;
//        r = NULL;
        Json::Value jRets;
        Json::Reader jReader;
        if (studyRe!= "") {
//            cout<<"study retData empty"<<endl;
            cout<<"***************study retData:"<<studyRe<<endl;
            if (jReader.parse(studyRe,jRets)) {
                Json::Value tmpArray;
                tmpArray = jRets["result"];
                cout<<"study result size:"<<tmpArray.size()<<endl;
                for (int i = 0; i < tmpArray.size(); ++i) {
                    Json::Value studyResultContent;
                    studyResultContent["toolNo"] = tmpArray[i]["toolnum"].asInt();
                    studyResultContent["value"] = tmpArray[i]["load"].asDouble();
                    studyResult.append(studyResultContent);
                }
            }

            dataRoot["fileName"] = redisMap["programName"];
            dataRoot["studyId"] = studyId;
            dataRoot["studyResult"] = studyResult;

            contentRoot["dest"] = "toolLife";
            contentRoot["order"] = 225;
            contentRoot["switchs"] = "on";
            contentRoot["frequency"] = 1;
            contentRoot["source"] = "";
            contentRoot["cmdId"] = "";
            contentRoot["level"] = 7;
            contentRoot["concurrent"] = "true";
            contentRoot["data"] = dataRoot.toStyledString();

            root["encode"] = false;
            root["id"] = "";
            root["machineNo"] = machineId;
            root["type"] = 20;
            root["order"] = 225;
            root["dest"] = "toolLife";
            root["content"] = contentRoot.toStyledString();

            string strStudyResultMsg = root.toStyledString();
            cout<<"test studyResult msg:"<<strStudyResultMsg<<endl;
            DEBUGLOG("test studyResult msg:"<<strStudyResultMsg);
            vcSendMsgs.push_back(strStudyResultMsg);
        }

    }
}

void alertToolProcess(string mode) {
    while (1) {
//        vcSendMsgs.push_back(gener)
        string recordVal = processToolVal(mode);
        recordVal = resultToolVal;
//        string recordVal = chRecordVal;
//        while (recordVal.substr(recordVal.length()-1,1) != "}") {
//            recordVal.pop_back();
//        }
        if (strprocessStatus == "error end") {
            recordVal = "";
            strprocessStatus = "";
        }

        //进行预警处理，生成预警报文
        Json::Value root;
        Json::Value contentRoot;
        Json::Value dataRoot;
        Json::Value characteristicValueRoot;
        Json::Value alarmDetailRoot;
        bool bAlarm = false;

        double *r;
//        r = recordVal.r;
        Json::Reader tmpReader;
        Json::Value tmpRoot;
        Json::Value tmpArray;
        if (recordVal != "") {
            if (tmpReader.parse(resultToolVal,tmpRoot)) {
                tmpArray = tmpRoot["result"];
            }
            cout<<"******************toolval："<<resultToolVal<<endl;
            cout<<"result array size:"<<tmpArray.size()<<endl;
            for (int i = 0; i < tmpArray.size(); ++i) {
                int toolNo = tmpArray[i]["toolnum"].asInt();
                double val = tmpArray[i]["load"].asDouble();
                if (val <  0) {
                    ERRORLOG(recordVal.c_str());
                    val = 0;
                }
                Json::Value valContent;
                valContent["toolNo"] = tmpArray[i]["toolnum"].asInt();
                valContent["value"] = val;
                characteristicValueRoot.append(valContent);

                if (programName != redisMap["programName"]) {

                } else {
                    if (val != 0) {
                        if ((!maxLimMap.empty())&&(maxLimMap.count(toolNo))) {
                            if (val > (maxLimMap[toolNo]*maxSensMap[toolNo])) {
                                bAlarm = true;
                                Json::Value alarmContent;
                                alarmContent["toolNo"] = toolNo;
                                alarmContent["detail"] = "超过预警上限！";
                                alarmDetailRoot.append(alarmContent);
                            }
                        }

                        if ((!minLimMap.empty())&&(minLimMap.count(toolNo))) {
                            if (val < (minLimMap[toolNo]*minSensMap[toolNo])) {
                                bAlarm = true;
                                Json::Value alarmContent;
                                alarmContent["toolNo"] = toolNo;
                                alarmContent["detail"] = "低于预警下限！";
                                alarmDetailRoot.append(alarmContent);
                            }
                        }
                    }
                }

            }

            if (bAlarm) {
                dataRoot["hasAlarm"] = 1;
                //生成向机床报警暂停信息
                Json::Value alarmRoot;
                Json::Value writeValRoot;

                alarmRoot["type"] = -99;
                alarmRoot["order"] = 2;

                map<string,string>::iterator iterator1;
                for (iterator1=addrMap.begin();iterator1!=addrMap.end();iterator1++) {
                    Json::Value arrayObj;
                    arrayObj[iterator1->second] = "1";
                    writeValRoot.append(arrayObj);
                }
                alarmRoot["writeValue"] = writeValRoot;
                cout<<"*****set alarmkey msg:"<<alarmRoot.toStyledString()<<endl;
                vcSendLocalMsgs.push_back(alarmRoot.toStyledString());
            } else {
                dataRoot["hasAlarm"] = 0;
            }
            dataRoot["alarmDetail"] = alarmDetailRoot;
            dataRoot["characteristicValue"] = characteristicValueRoot;
            dataRoot["fileName"] = redisMap["programName"];
            dataRoot["time"] = getCurrentTime();

            contentRoot["data"] = dataRoot.toStyledString();
            contentRoot["dest"] = "toolLife";
            contentRoot["order"] = 226;
            contentRoot["switchs"] = "on";
            contentRoot["frequency"] = 1;
            contentRoot["source"] = "";
            contentRoot["cmdId"] = "";
            contentRoot["level"] = 7;
            contentRoot["concurrent"] = "true";

            root["content"] = contentRoot.toStyledString();
            root["encode"] = false;
            root["id"] = "";
            root["machineNo"] = machineId;
            root["type"] = 20;
            root["order"] = 226;
            root["dest"] = "toolLife";

            //生成需学习结果报文
            if (studyStatus == "study") {
                studyProcess(recordVal,studyId);
                studyStatus  = "finish";

            } else {
                string strValDataMsg = root.toStyledString();
                vcSendMsgs.push_back(strValDataMsg);
            }
        }
    }


}

string getConfigContent(string filePth) {
    ifstream configStream(filePth);
    if (!configStream.is_open()) {
        cout<<"error open config file:"<<filePth<<endl;
        DEBUGLOG("error open config file:"<<filePth);
        return "";
    } else {
        stringstream buffer;
        buffer<<configStream.rdbuf();
        string strConfig(buffer.str());
        return strConfig;
    }
}

//读取本地阈值配置
void initLocalConfig(string content) {
    maxLimMap.clear();
    maxSensMap.clear();
    minLimMap.clear();
    minSensMap.clear();

    if (content != "") {
        Json::Reader reader;
        Json::Value dataRoot;

        if (reader.parse(content,dataRoot)) {
            programName = dataRoot["fileName"].asString();
            configMachineNo = dataRoot["deviceNo"].asString();
            Json::Value characteristicMax = dataRoot["characteristicMax"];
            Json::Value characteristicMin = dataRoot["characteristicMin"];
            if (characteristicMax != NULL) {
                for (int i = 0; i < characteristicMax.size(); ++i) {
                    if (characteristicMax[i]["value"].asDouble() >= 0) {
                        maxLimMap[characteristicMax[i]["toolNo"].asInt()] = characteristicMax[i]["value"].asDouble();
                        maxSensMap[characteristicMax[i]["toolNo"].asInt()] = characteristicMax[i]["sens_ty"].asDouble();
                    }

                }
            }

            if (characteristicMin != NULL) {
                for (int i = 0; i < characteristicMin.size(); ++i) {
                    if (characteristicMin[i]["value"].asDouble() >= 0) {
                        minLimMap[characteristicMin[i]["toolNo"].asInt()] = characteristicMin[i]["value"].asDouble();
                        minSensMap[characteristicMin[i]["toolNo"].asInt()] = characteristicMin[i]["sens_ty"].asDouble();
                    }

                }
            }

            Json::Value groupedTool = dataRoot["groupedTool"];
            if (groupedTool != NULL) {
                cout<<"grouped tool size:"<<groupedTool.size()<<endl;
                for (int i = 0; i < groupedTool.size(); ++i) {
                    Json::Value toolArray = groupedTool[i];
                    double sameMaxVal;
                    double sameMinVal;
                    double sameMaxSensVal;
                    double sameMinSensVal;
                    cout<<"tool Array size:"<<toolArray.size()<<endl;
                    for (int j = 0; j < toolArray.size(); ++j) {
                        int iToolNo = toolArray[j].asInt();
                        //刀组阈值上限设置
                        if (maxLimMap.count(iToolNo)) {
                            sameMaxVal = maxLimMap[iToolNo];
                            for (int k = 0; k < toolArray.size(); ++k) {
                                maxLimMap[toolArray[k].asInt()] = sameMaxVal;
                            }
                        }

                        //刀组阈值下限设置
                        if (minLimMap.count(iToolNo)) {
                            sameMinVal = minLimMap[iToolNo];
                            for (int k = 0; k < toolArray.size(); ++k) {
                                minLimMap[toolArray[k].asInt()] = sameMinVal;
                            }
                        }

                        //刀组灵敏度上限设置
                        if (maxSensMap.count(iToolNo)) {
                            sameMaxSensVal = maxSensMap[iToolNo];
                            for (int k = 0; k < toolArray.size(); ++k) {
                                maxSensMap[toolArray[k].asInt()] = sameMaxSensVal;
                            }
                        }

                        //刀组灵敏度下限设置
                        if (minSensMap.count(iToolNo)) {
                            sameMinSensVal = minSensMap[iToolNo];
                            for (int k = 0; k < toolArray.size(); ++k) {
                                minSensMap[toolArray[k].asInt()] = sameMinSensVal;
                            }
                        }
                    }
                }
                map<int,double>::iterator itmaxLim;
                for (itmaxLim = maxLimMap.begin(); itmaxLim != maxLimMap.end(); ++itmaxLim) {
                    cout<<"lim toolNo:"<<itmaxLim->first<<endl;
                    cout<<"max lim data:"<<maxLimMap[itmaxLim->first]*maxSensMap[itmaxLim->first]<<endl;
                    cout<<"min lim data:"<<minLimMap[itmaxLim->first]*minSensMap[itmaxLim->first]<<endl;
                    cout<<"************"<<endl;
                }
            }
            //忽略预警刀号设置
            Json::Value noAlarmTool = dataRoot["noAlarmTool"];
            if (noAlarmTool != NULL) {
                for (int i = 0; i < noAlarmTool.size(); ++i) {
                    int noAlarmToolNo = noAlarmTool[i].asInt();
                    vcNoAlarmTool.push_back(noAlarmToolNo);
                    if (maxLimMap.count(noAlarmToolNo)) {
                        maxLimMap.erase(noAlarmToolNo);
                        maxSensMap.erase(noAlarmToolNo);
                    }
                    if (minLimMap.count(noAlarmToolNo)) {
                        minLimMap.erase(noAlarmToolNo);
                        minSensMap.erase(noAlarmToolNo);
                    }
                 }
            }
        }
    }
}

//前端配置
void initToolConfig(string content) {
    if (content != "") {
        HhKeysMap.clear();
        HlKeysMap.clear();
        Json::Reader reader;
        Json::Value root;
        Json::Value hhKeys;
        Json::Value hlKeys;
        Json::Value machineStatusRoot;
        Json::Value addrRoot;
        if (reader.parse(content,root)) {
            dllPath = root["dllPath"].asString();
            strHhKeyVal = root["hhkey"].asString();
            strHlKeyVal = root["hlkey"].asString();
            hhKeys = root["hhkeyRedis"];
            hlKeys = root["hlkeyRedis"];
            machineStatusRoot = root["machineStatus"];
            addrRoot = root["addr"];

//            HlKeysMap["programName"] = hlKeys["programName"].asString();
//            HlKeysMap["countStatus"] = hlKeys["countStatus"].asString();
//            HlKeysMap["mahineStatusKey"] = hlKeys["mahineStatusKey"].asString();
//            HlKeysMap["resetKey"] = hlKeys["resetKey"].asString();
//
//            HhKeysMap["load"] = hhKeys["load"].asString();
////            HhKeysMap["rpmKey"] = hhKeys["rpmKey"].asString();
//            HhKeysMap["timeStamp"] = hhKeys["timeStamp"].asString();
//            HhKeysMap["programStartTime"] = hhKeys["programStartTime"].asString();
//            HhKeysMap["programEndTime"] = hhKeys["programEndTime"].asString();
//            HhKeysMap["toolNo"] = hhKeys["toolNo"].asString();

            Json::Value::Members HlMems = hlKeys.getMemberNames();
            Json::Value::Members::iterator HlIt;
            for (HlIt = HlMems.begin();HlIt != HlMems.end();++HlIt) {
                HlKeysMap[*HlIt] = hlKeys[*HlIt].asString();
            }

            Json::Value::Members HhMems = hhKeys.getMemberNames();
            Json::Value::Members::iterator HhIt;
            for (HhIt = HhMems.begin();HhIt != HhMems.end();++HhIt) {
                HhKeysMap[*HhIt] = hhKeys[*HhIt].asString();
            }

            machineStatusMap["work"] = machineStatusRoot["work"].asString();
            machineStatusMap["free"] = machineStatusRoot["free"].asString();
            machineStatusMap["hold"] = machineStatusRoot["hold"].asString();

            addrMap["alarm"] = addrRoot["alarm"].asString();
            jAddrVal = root["readAddr"];
        }
    }

    cout<<"HhKeyMap:"<<endl;
    DEBUGLOG("HhKeyMap:");
    map<string,string>::iterator it;
    for (it = HhKeysMap.begin();it!=HhKeysMap.end();++it) {
        cout<<it->first<<" "<<it->second<<endl;
        DEBUGLOG(it->first<<" "<<it->second);
    }

    cout<<"HlKeyMap:"<<endl;
    DEBUGLOG("HlKeyMap:");
    map<string,string>::iterator it1;
    for (it1 = HlKeysMap.begin();it1!=HlKeysMap.end();++it1) {
        cout<<it1->first<<" "<<it1->second<<endl;
        DEBUGLOG(it1->first<<" "<<it1->second);
    }
}

//盒子公共配置，获取盒子machineId
int initCommConfig(string path) {
    Config config;
    if (config.FileExist(path)) {
        config.ReadFile(path);
        machineId = config.Read<string>("machineno","");
        cout<<"***********::"<<machineId<<endl;
        DEBUGLOG("***********::"<<machineId);
        return 0;
    } else {
        cout<<"ini config file not exist!"<<endl;
        DEBUGLOG("ini config file not exist!");
        return -1;
    }
}


void processMsg(string msgContent) {
    Json::Reader reader;
    Json::Value root;
    if (reader.parse(msgContent,root)) {
        int iOrder = root["order"].asInt();
        if ((iOrder == 222)||(iOrder == 223)) {
            Json::Reader contentReader;
            Json::Value contentRoot;
            Json::Reader dataReader;
            Json::Value dataRoot;
            string strContent = root["content"].asString();
            if(contentReader.parse(strContent,contentRoot)) {
                string strData = contentRoot["data"].asString();
                if (strData != "") {
                    if (dataReader.parse(strData,dataRoot)) {
                    }
                }

            }
            //阈值配置信息报文
            if (iOrder == 222) {
                //收到阈值信息后先将信息写入本地文件中保存
                if (dataRoot != NULL) {
                    string configData = dataRoot.toStyledString();
                    ofstream out("toolVal.json");
                    out<<configData<<endl;
                    out.close();

                    //处理阈值配置信息
                    initLocalConfig(configData);
                }
            } else if (iOrder == 223) {
                //开始学习，终止学习报文
                if (dataRoot != NULL) {
                    studyId = dataRoot["studyId"].asString();
                    string studyLimit = dataRoot["studyLimit"].asString();
                    studyStatus = dataRoot["action"].asString();
                    if (studyStatus == "study") {
                        //开始学习
//                    std::thread thStudyProcess(studyProcess,"study",studyId);
//                    thStudyProcess.detach();
                    }
                    //生成回复报文
                    Json::Value root;
                    Json::Value contentRoot;
                    Json::Value dataRoot;

                    dataRoot["time"] = getCurrentTime();
                    dataRoot["studyId"] = studyId;

                    contentRoot["dest"] = "toolLife";
                    contentRoot["order"] = 224;
                    contentRoot["switchs"] = "on";
                    contentRoot["frequency"] = 1;
                    contentRoot["source"] = "";
                    contentRoot["cmdId"] = "";
                    contentRoot["level"] = 7;
                    contentRoot["concurrent"] = "true";
                    contentRoot["data"] = dataRoot.toStyledString();

                    root["encode"] = false;
                    root["id"] = "";
                    root["machineNo"] = machineId;
                    root["type"] = 20;
                    root["order"] = 224;
                    root["dest"] = "toolLife";
                    root["content"] = contentRoot.toStyledString();

                    vcSendMsgs.push_back(root.toStyledString());
                }
            } else {

            }
        } else if (iOrder == 89) {
            Json::Reader contentReader;
            Json::Value contentRoot;
            string strContent = root["content"].asString();
            if (contentReader.parse(strContent,contentRoot)) {
//                strUploadToken = contentRoot["uploadToken"].asString();
//                strBizId = contentRoot["bizId"].asString();
//                strUploadUrl = contentRoot["uploadUrl"].asString();
                string strData = contentRoot["data"].asString();
                Json::Reader dataReader;
                Json::Value dataRoot;
                if (dataReader.parse(strData,dataRoot)) {
                    strUploadToken = dataRoot["uploadToken"].asString();
                    strBizId = dataRoot["bizId"].asString();
                    strUploadUrl = dataRoot["uploadUrl"].asString();
                }
            }
        }
    } else {

    }

}

//获取阈值报文
string getValConfigMsg() {
    Json::Value root;
    Json::Value contentRoot;
    Json::Value dataRoot;

    dataRoot["time"] = getCurrentTime();

    contentRoot["data"] = dataRoot.toStyledString();
    contentRoot["dest"] = "toolLife";
    contentRoot["order"] = 221;
    contentRoot["switchs"] = "on";
    contentRoot["frequency"] = 1;
    contentRoot["source"] = "";
    contentRoot["cmdId"] = "";
    contentRoot["level"] = 7;
    contentRoot["concurrent"] = "true";

    root["content"] = contentRoot.toStyledString();
    root["encode"] = false;
    root["id"] = "";
    root["machineNo"] = machineId;
    root["type"] = 20;
    root["order"] = 221;
    root["dest"] = "toolLife";

    return root.toStyledString();
}

string readAddrKeyVal(Json::Value val,string keyName) {
    string retStr = "";
//    cout<<"*************read addr:"<<val.size()<<endl;
    if (val != NULL) {
        for (int i = 0; i < val.size(); ++i) {
            Json::Value::Members members = val[i].getMemberNames();
            for (Json::Value::Members::iterator iter = members.begin();iter!=members.end();iter++) {
                if ((*iter) == keyName) {
//                    cout<<"key:"<<keyName<<"exist"<<endl;
                    string readType = val[i]["bt"].asString();
                    string readBit = val[i]["bit"].asString();
                    string effectVal = val[i]["val"].asString();
                    if (readType == "true") {
//                        string mapVal = redisMap["keyName"];
//                        cout<<"type true"<<endl;
                        int iBit;
                        int iMapVal;
                        stringstream ssInt;
                        stringstream ssMapVal;
                        ssInt<<readBit;
                        ssInt>>iBit;
                        ssMapVal<<redisMap[keyName];
                        ssMapVal>>iMapVal;
//                        cout<<"resetkey val:"<<iMapVal<<endl;
//                        cout<<(iMapVal&(1<<iBit))<<endl;
                        if((iMapVal&(1<<iBit)) != 0) {
                            retStr = "1";
//                            cout<<"ret resetVal:"<<retStr<<endl;
                        } else {
                            retStr = "0";
                        }
                    } else {
                        if (effectVal == redisMap[keyName]) {
                            retStr = "1";
                        } else {
                            retStr = "0";
                        }
                    }
                }
            }
        }
    }
    return retStr;
}

void resetValProcess() {
    while (1) {
        this_thread::sleep_for(chrono::milliseconds(5));
        string tmpResetVal = "";
        while (1) {
            this_thread::sleep_for(chrono::milliseconds(10));
            if (!redisMap.empty()) {

                if (redisMap.count("resetKey")&&(readAddrKeyVal(jAddrVal,"resetKey") == "1")) {
                    tmpResetVal = "1";
                    break;
                }
            }
        }

        while (1) {
            this_thread::sleep_for(chrono::milliseconds(10));
            if ((tmpResetVal != "")&&(tmpResetVal == "1")) {
                if (readAddrKeyVal(jAddrVal,"resetKey") == "0") {
                    tmpResetVal = "0";
                    //发送reset报警信息
                    Json::Value tmpAlarmRoot;
                    Json::Value writeArray;
                    tmpAlarmRoot["type"] = -99;
                    tmpAlarmRoot["order"] = 2;
                    Json::Value tmpObj;
                    tmpObj[addrMap["alarm"]] = "0";
                    writeArray.append(tmpObj);
                    tmpAlarmRoot["writeValue"] = writeArray;
                    cout<<"************local resetkey msg:"<<tmpAlarmRoot.toStyledString()<<endl;
                    vcSendLocalMsgs.push_back(tmpAlarmRoot.toStyledString());
                    break;
                }
            }
        }
    }

}

void reset3000process() {
    while (1) {
        this_thread::sleep_for(chrono::milliseconds(5));
        if (redisMap.count("resetKey")) {
            if (redisMap["resetKey"] == "1") {
                this_thread::sleep_for(chrono::milliseconds(1000));
                Json::Value tmpAlarmRoot;
                Json::Value writeArray;
                tmpAlarmRoot["type"] = -99;
                tmpAlarmRoot["order"] = 2;
                Json::Value tmpObj;
                tmpObj[addrMap["alarm"]] = "0";
                writeArray.append(tmpObj);
                tmpAlarmRoot["writeValue"] = writeArray;
                cout<<"************local 3000 resetkey msg:"<<tmpAlarmRoot.toStyledString()<<endl;
                vcSendLocalMsgs.push_back(tmpAlarmRoot.toStyledString());
            }
        }
    }
}

void alarmToMachine() {
    Json::Value alarmRoot;
    Json::Value writeValRoot;

    alarmRoot["type"] = -99;
    alarmRoot["order"] = 2;

    map<string,string>::iterator iterator1;
    for (iterator1=addrMap.begin();iterator1!=addrMap.end();iterator1++) {
        Json::Value arrayObj;
        arrayObj[iterator1->second] = "1";
        writeValRoot.append(arrayObj);
    }
    alarmRoot["writeValue"] = writeValRoot;
    cout<<"*****set alarmkey msg:"<<alarmRoot.toStyledString()<<endl;
    vcSendLocalMsgs.push_back(alarmRoot.toStyledString());
}

void rpmMonitorProcess() {
    int formerRpm = 0;
    int currentRpm = 0;
    int countTimes = 0;
    while (1) {
        this_thread::sleep_for(chrono::milliseconds(30));
        if (!redisMap.empty()) {
            if (redisMap["rpmKey"] != "0") {
                countTimes++;
                stringstream ssRpm;
                ssRpm<<redisMap["rpmKey"];
                if (countTimes%2 == 1) {
                    ssRpm>>formerRpm;
                } else {
                    ssRpm>>currentRpm;

                    int rpmData = currentRpm - formerRpm;
                    if (rpmData > 0) {

                    } else if (rpmData < 0) {

                    } else {

                    }
                }

            }
        }
    }
}

void writeRedisBeat() {
    while (1) {
        this_thread::sleep_for(chrono::seconds(30));
        cout<<"time:"<<getSysTime()<<endl;
        redisWriteClient.Set("toolLife_connect",getSysTime());
    }
}

void cleanFile() {
    while (1) {
        this_thread::sleep_for(chrono::seconds(1800));
        vector<string> fileNames = fileCtl.readFileList("/tmp/toolLife/");
        string currTime = getSysTime();
        stringstream ssCTime;
        long lTime;
        ssCTime<<currTime;
        ssCTime>>lTime;
        //delete zip pack
        for (int i = 0; i < fileNames.size(); ++i) {
            vector<string> splitType = split(fileNames[i],".");
            if (splitType[1] == "zip") {
                vector<string> zipNames = split(splitType[0],"_");
                string zipTime = zipNames[1];
                stringstream ssZipTime;
                long lZipTime;
                ssZipTime<<zipTime;
                ssZipTime>>lZipTime;

                if ((lZipTime + 172800)<lTime) {
                    fileCtl.deleteFile("/tmp/toolLife/" + fileNames[i]);
                }
            } else if (splitType[1] == "txt") {
                vector<string> txtNames = split(splitType[0],"_");
                string txtTime = txtNames[0];
                long lTxtTime;
                stringstream ssTxtTime;
                ssTxtTime<<txtTime;
                ssTxtTime>>lTxtTime;

                if ((lTxtTime + 172800)<lTime) {
                    fileCtl.deleteFile("/tmp/toolLife/" + fileNames[i]);
                }
            } else {

            }
        }
        //delete origin files
        fileNames.clear();
    }
}

//void testTime() {
//    while (1) {
//        this_thread::sleep_for(chrono::seconds(1));
//        string strTime = getSysTime();
//        cout<<"time:"<<strTime<<"|"<<strTime.length()<<endl;
//    }
//
//}

int main(int argc,char *argv[]) {
    if (argc > 1) {
        if (strcmp(argv[1], "-v") == 0) {
            cout<<"Version V0.1"<<endl;
            return 0;
        }
    }

#ifdef WIN32
#else
    fileCtl.createPath("/home/i5/data/file/");
    fileCtl.createPath("/tmp/toolLife/");
    fileCtl.createPath("/home/i5/data/logs/toolLife/");
#endif

    //初始化配置文件
    //machineId配置
//    programName = "test.iso";
#ifdef WIN32
    initCommConfig("common.properties");
//    //获取阈值配置文件信息
    string tmpToolcontent = getConfigContent("toolVal.json");
    string tmpToolLifeContent = getConfigContent("toolLife.json");
#else
    if (initCommConfig("/home/i5/config/common/common.properties") != 0) {
        return -1;
    }
//    //获取阈值配置文件信息
    string tmpToolcontent = getConfigContent("/home/i5/config/toolLife/toolVal.json");
    string tmpToolLifeContent = getConfigContent("/home/i5/config/toolLife/toolLife.json");
    if (tmpToolLifeContent == "") {
        DEBUGLOG("toolLife.json file not exist!");
        return -1;
    }
#endif

    if (!Log::instance().open_log())
    {
        std::cout << "Log::open_log() failed" << std::endl;
    }


    if (tmpToolcontent != "") {
        initLocalConfig(tmpToolcontent);
    }

    //获取

    if (tmpToolLifeContent != "") {
        initToolConfig(tmpToolLifeContent);
    }

    //初始化算法库
#ifdef WIN32
    if (dllPath != "") {
        HINSTANCE winDll = LoadLibrary(_T(dllPath));
        if (winDll != NULL) {
            initFun = (pInitFun)GetProcAddress(winDll,"initial");
            feedFun = (pFeedFun)GetProcAddress(winDll,"feed");
            resultFun = (pResultFun)GetProcAddress(winDll,"loadresult");
        }
    }
#else
    void *plib = dlopen(dllPath.c_str(),RTLD_NOW | RTLD_GLOBAL);
    if (!plib) {
        cout<<"error msg:"<<dlerror()<<endl;
        DEBUGLOG("error msg:"<<dlerror());
    } else {
        cout<<"p to fun"<<endl;
        char *strError;
        initFun = (pInitFun)dlsym(plib,"initial");
        if ((strError = dlerror()) != NULL) {
            cout<<"p to fun error:"<<strError<<endl;
        }
        feedFun = (pFeedFun)dlsym(plib,"feed");
        if ((strError = dlerror()) != NULL) {
            cout<<"p to fun error:"<<strError<<endl;
        }
        resultFun = (pResultFun)dlsym(plib,"loadresult");
        if ((strError = dlerror()) != NULL) {
            cout<<"p to fun error:"<<strError<<endl;
        }
    }
//    int tmpStatus = dlclose(plib);
//    cout<<"dl close stastus:"<<tmpStatus<<endl;
//    cout<<"dlopen finish"<<endl;

#endif

    //本地redis连接
    vcRecvMsgs.clear();
    vcSendMsgs.clear();
    vcSendLocalMsgs.clear();
    redisMap.clear();
    bool redisStatus = false;
    int redisCount = 0;
    while (!redisStatus) {
        redisStatus = redisClient.Connect(redisAddr,redisPort);
        cout<<"redis connect status:"<<redisStatus<<endl;
        DEBUGLOG("redis connect status:"<<redisStatus);
        redisCount++;
        this_thread::sleep_for(chrono::seconds(1));
        if (redisCount == 10) {
            cout<<"toolLife local redis connect fail"<<endl;
            DEBUGLOG("toolLife local redis connect fail");
            redisStatus = true;
            return -1;
        }
    }

    bool redisWriteStatus = false;
    int redisWriteCount = 0;
    while (!redisWriteStatus) {
        redisWriteStatus = redisWriteClient.Connect(redisAddr,redisPort);
        cout<<"redis write connect status:"<<redisStatus<<endl;
        DEBUGLOG("redis connect status:"<<redisStatus);
        redisWriteCount++;
        this_thread::sleep_for(chrono::seconds(1));
        if (redisWriteCount == 10) {
            cout<<"toolLife local redis connect fail"<<endl;
            DEBUGLOG("toolLife local redis connect fail");
            redisWriteStatus = true;
            return -1;
        }
    }

    //mqtt broker 连接
    mqtt::async_client client(mqttAddr,strMqttPort);
    mqtt::connect_options opts;
    opts.set_keep_alive_interval(20);
    opts.set_clean_session(true);
    callback cb;
    client.set_callback(cb);
    cout<<"mqtt init finish********"<<endl;


   try {
       cout << "\nConnecting..." << endl;
       mqtt::token_ptr conntok = client.connect(opts);
       cout << "Waiting for the connection..." << endl;
       conntok->wait();
       client.start_consuming();
       client.subscribe(SUBTOPIC,0)->wait();
       cout<<"subscr topic toolLife ok!"<<endl;
       client.subscribe(PUBTOPIC,0)->wait();
       cout<<"subscr topic "<<PUBTOPIC<<" ok!"<<endl;
       string tmpLocalTopic = localTopic + machineId;
//       client.subscribe(tmpLocalTopic,0)->wait();
//       cout<<"subscr topic "<<tmpLocalTopic<<" ok!"<<endl;
       cout << "  ...OK" << endl;
       cout<<"callback member conn status:"<<cb.bConnStatus<<endl;

   } catch (const mqtt::exception &exc) {
       cerr << exc.what() << endl;
       return 1;
   }

   int mqttCount = 0;
   while(!cb.bConnStatus) {
       this_thread::sleep_for(chrono::seconds(1));
       mqttCount++;
       if(mqttCount == 10) {
           cout<<"mqtt connect fail"<<endl;
           return 1;
       }
   }

//   alarmToMachine();

//启动redis数据采集线程
    std::thread thCollectData(collectDataProcess);
    thCollectData.detach();

    //生成获取阈值报文消息
    vcSendMsgs.push_back(getValConfigMsg());

    //启动预警信息计算处理
    std::thread thAlertToolProcess(alertToolProcess,"alarmProcess");
    thAlertToolProcess.detach();

    //处理reset报警状态
    std::thread thResetAlarm(resetValProcess);
    thResetAlarm.detach();

    //写入redis心跳
    std::thread thWriteRedisBeat(writeRedisBeat);
    thWriteRedisBeat.detach();

    //定时清理文件
    std::thread thCleanFile(cleanFile);
    thCleanFile.detach();

//    std::thread thTimeTest(testTime);
//    thTimeTest.detach();

   //处理消息 发送消息
   while ((cb.bConnStatus)) {
       this_thread::sleep_for(chrono::milliseconds(100));
       //处理消息
       if (!vcRecvMsgs.empty()) {
           vector<string>::iterator it = vcRecvMsgs.begin();
           string tmpStrMsg = (*it);
           vcRecvMsgs.erase(it);
           //处理接收消息
           processMsg(tmpStrMsg);
       }

       //发送消息
       if (cb.bConnStatus) {
           if (!vcSendMsgs.empty()) {
               //发送消息
               vector<string>::iterator it = vcSendMsgs.begin();
               string tmpSendMsg = (*it);
               vcSendMsgs.erase(it);
               mqtt::message_ptr msg = mqtt::make_message(PUBTOPIC,tmpSendMsg);
               msg->set_qos(0);
               client.publish(msg)->wait_for(2);
               cout<<"pub cloud msg ok"<<endl;
           }

           if (!vcSendLocalMsgs.empty()) {
               //发送消息
               vector<string>::iterator it = vcSendLocalMsgs.begin();
               string tmpSendMsg = (*it);
               vcSendLocalMsgs.erase(it);
               string tmpLocalTopic = localTopic + machineId;
               cout<<"local topic:"<<tmpLocalTopic<<endl;
               mqtt::message_ptr msg = mqtt::make_message(tmpLocalTopic,tmpSendMsg);
               msg->set_qos(0);
               client.publish(msg)->wait_for(2);
               cout<<"pub local msg ok"<<endl;
           }

           //发送上传文件消息
           if (!vcSendLocalUploadMsgs.empty()) {
               vector<string>::iterator it = vcSendLocalUploadMsgs.begin();
               string tmpSendMsg = (*it);
               vcSendLocalUploadMsgs.erase(it);
               string uploadTopic = "FileUpload";
               cout<<"local topic:"<<uploadTopic<<endl;
               cout<<"local uplaod msg:"<<tmpSendMsg<<endl;
               mqtt::message_ptr msg = mqtt::make_message(uploadTopic,tmpSendMsg);
               msg->set_qos(0);
               client.publish(msg)->wait_for(2);
               cout<<"pub local upload msg ok"<<endl;
           }
       }
   }


//
//    cout<<"input q to exit"<<endl;
//    while (std::tolower(std::cin.get()) != 'q') {
//        bool redisStatus = redisClient.Connect("127.0.0.1",6379);
//        cout<<"redisConn status:"<<redisStatus<<endl;
//        cout<<"redis connetct status:"<<redisClient.CheckStatus()<<endl;
//    }

   try {
//       client.unsubscribe()->wait();
       client.stop_consuming();
       client.disconnect()->wait();
       cout<<"discontent ok"<<endl;
   } catch (mqtt::exception &exc) {
       cerr << exc.what() << endl;
       return 1;
   }
    try {
        redisClient.Disconnect();
        redisWriteClient.Disconnect();
    }catch (exception e) {
        cerr << e.what()<<endl;
    }

    return 0;
}