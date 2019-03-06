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
#include <vector>
#include <time.h>
#include <map>

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

//程序名
string programName = "";

//string programStartPoint = "";
//string programEndPoint = "";

string dllPath = "tooldll.dll";
string redisAddr = "127.0.0.1";
int redisPort = 6379;
string mqttAddr = "127.0.0.1";
string strMqttPort = "1883";

vector<string> vcRecvMsgs;
vector<string> vcSendMsgs;

string strHhKeyVal = "2010";
string strHlKeyVal = "2011";

map<string,string> HhKeysMap;
map<string,string> HlKeysMap;
map<string,string> machineStatusMap;

map<string,string> redisMap;

map<int,double> maxLimMap;
map<int,double> maxSensMap;
map<int,double> minLimMap;
map<int,double> minSensMap;
vector<int> vcNoAlarmTool;
//map<int,double> alertMap;

CRedisClient redisClient;

typedef double *(*pInitFun)();
typedef int (*pFeedFun)(int toolNum, double load,double *p);
typedef Results (*pResultFun)(double *p);

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
//        DEBUGLOG(cause);
        bConnStatus = true;
        int ret;
    }

    /**
     * This method is called when the connection to the server is lost.
     * @param cause
     */
    virtual void connection_lost(const string &cause) {
        cout << "callback->connection_lost..." << cause << endl;
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
        std::cout << "Message arrived :" << std::endl;
        std::cout << "\ttopic: '" << msg->get_topic() << "'" << std::endl;
        std::cout << "\tpayload: '" << msg->to_string() << "'\n" << std::endl;
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

void collectDataProcess() {
    cout<<"collect data thread running"<<endl;
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
                    map<string,string>::iterator it;
                    for (it=HhKeysMap.begin();it!=HhKeysMap.end();++it) {
                        redisMap[it->first] = root[it->second].asString();
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
                    map<string,string>::iterator it;
                    for (it=HlKeysMap.begin();it!=HlKeysMap.end();++it) {
                        redisMap[it->first] = root[it->second].asString();
//                        cout<<it->second<<" val "<<root[it->second].asString()<<endl;
                    }

                }
            }
        }
    }
}

void programNameAlarm(string currentName) {
    if (currentName != "") {
        if (currentName != programName) {
            Json::Value root;
            Json::Value contentRoot;
            Json::Value dataRoot;

            dataRoot["time"] = getCurrentTime();
            dataRoot["errorId"] = GetGuid();
            dataRoot["detail"] = "current program name error";

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


Results processToolVal(string flag) {
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

    double *tmpPointer;
    double *tmpR;
    Results re;
    tmpPointer = initFun();

    long tmpCount = 0;
    double tmpSum = 0;
    while (1) {
        this_thread::sleep_for(chrono::milliseconds(10));
        if (!redisMap.empty()) {
            if (redisMap["countStatus"] == "false") {
                if (tmpStartPoint != "") {
                    if (redisMap["programStartTime"] != tmpStartPoint) {
//                        cout<<"counting..."<<endl;
                        if (flag == "study") {
                            if (studyStatus == "abort") {
                                break;
                            }
                        }

                        if (redisMap["programEndTime"] != tmpEndPoint) {
                            cout<<"count process end"<<endl;
                            break;
                        }
                        //获取redis中存储的数据
                        if (redisMap.count("toolNo")&&redisMap.count("load")) {
                            tmpCount++;
                            stringstream ssToolNo;
                            stringstream ssLoad;
                            int toolNo;
                            double tmpload;
                            ssToolNo<<redisMap["toolNo"];
                            ssToolNo>>toolNo;
                            ssLoad<<redisMap["load"];
                            ssLoad>>tmpload;
                            feedFun(toolNo,tmpload,tmpPointer);
                            tmpSum = tmpSum + tmpload;
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
                                feedFun(toolNo,tmpload,tmpPointer);
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
    double tmpDresult = tmpSum/tmpCount;
    cout<<"********test result:"<<tmpDresult<<endl;
    re = resultFun(tmpPointer);
//    delete tmpPointer;
//    tmpPointer = NULL;
    return re;
}

void studyProcess(string mode,string studyId) {
    Results studyRe = processToolVal(mode);
    if (studyStatus == "abort") {
        cout<<"study process abort"<<endl;
    } else {
        //生成阈值报文回复
        Json::Value root;
        Json::Value contentRoot;
        Json::Value dataRoot;
        Json::Value studyResult;

        double *r;
        r = studyRe.r;
        for (int i = 0; i < studyRe.n; ++i) {
            int toolNo = r[i*2];
            double val = r[i*2]+1;
            Json::Value studyResultContent;
            studyResultContent["toolNo"] = toolNo;
            studyResultContent["value"] = val;
            studyResult.append(studyResultContent);
        }
        delete r;
        r = NULL;

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

        vcSendMsgs.push_back(strStudyResultMsg);
    }
}

void alertToolProcess(string mode) {
    while (1) {
        Results recordVal = processToolVal(mode);
        //进行预警处理，生成预警报文
        Json::Value root;
        Json::Value contentRoot;
        Json::Value dataRoot;
        Json::Value characteristicValueRoot;
        Json::Value alarmDetailRoot;
        bool bAlarm = false;

        double *r;
        r = recordVal.r;
        for (int i = 0; i < recordVal.n; ++i) {
            int toolNo = r[i*2];
            double val = r[i*2] + 1;
            Json::Value valContent;
            valContent["toolNo"] = toolNo;
            valContent["value"] = val;
            characteristicValueRoot.append(valContent);

            if ((!maxLimMap.empty())&&(maxLimMap.count(toolNo))) {
                if (val > (maxLimMap[toolNo]*maxSensMap[toolNo])) {
                    bAlarm = true;
                    Json::Value alarmContent;
                    alarmContent["toolNo"] = toolNo;
                    alarmContent["detail"] = "over maxLimit";
                    alarmDetailRoot.append(alarmContent);
                }
            }

            if ((!minLimMap.empty())&&(minLimMap.count(toolNo))) {
                if (val < (minLimMap[toolNo]*minSensMap[toolNo])) {
                    bAlarm = true;
                    Json::Value alarmContent;
                    alarmContent["toolNo"] = toolNo;
                    alarmContent["detail"] = "under minLimit";
                    alarmDetailRoot.append(alarmContent);
                }
            }
        }

        if (bAlarm) {
            dataRoot["hasAlarm"] = 1;
        } else {
            dataRoot["hasAlarm"] = 0;
        }
        dataRoot["alarmDetail"] = alarmDetailRoot;
        dataRoot["characteristicValue"] = characteristicValueRoot;
        dataRoot["fileName"] = programName;
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

        string strValDataMsg = root.toStyledString();
        vcSendMsgs.push_back(strValDataMsg);
    }


}

string getConfigContent(string filePth) {
    ifstream configStream(filePth);
    if (!configStream.is_open()) {
        cout<<"error open config file:"<<filePth<<endl;
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
            string fileName = dataRoot["fileName"].asString();
            string deviceNo = dataRoot["deviceNo"].asString();
            Json::Value characteristicMax = dataRoot["characteristicMax"];
            if (characteristicMax != NULL) {
                for (int i = 0; i < characteristicMax.size(); ++i) {
                    maxLimMap[characteristicMax[i]["toolNo"].asInt()] = characteristicMax[i]["value"].asDouble();
                    maxSensMap[characteristicMax[i]["toolNo"].asInt()] = characteristicMax[i]["sens_ty"].asDouble();
                }
            }

            Json::Value groupedTool = dataRoot["groupedTool"];
            if (groupedTool != NULL) {
                for (int i = 0; i < groupedTool.size(); ++i) {
                    Json::Value toolArray = groupedTool[i];
                    double sameMaxVal;
                    double sameMinVal;
                    double sameMaxSensVal;
                    double sameMinSensVal;
                    for (int j = 0; j < toolArray.size(); ++j) {
                        int iToolNo = toolArray[i].asInt();
                        //刀组阈值上限设置
                        if (maxLimMap.count(iToolNo)) {
                            sameMaxVal = maxLimMap[iToolNo];
                        } else {
                            maxLimMap[iToolNo] = sameMaxVal;
                        }
                        //刀组阈值下限设置
                        if (minLimMap.count(iToolNo)) {
                            sameMinVal = minLimMap[iToolNo];
                        } else {
                            minLimMap[iToolNo] = sameMinVal;
                        }
                        //刀组灵敏度上限设置
                        if (maxSensMap.count(iToolNo)) {
                            sameMaxSensVal = maxSensMap[iToolNo];
                        } else {
                            maxSensMap[iToolNo] = sameMaxSensVal;
                        }
                        //刀组灵敏度下限设置
                        if (minSensMap.count(iToolNo)) {
                            sameMinSensVal = minSensMap[iToolNo];
                        } else {
                            minSensMap[iToolNo] = sameMinSensVal;
                        }
                    }
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
        if (reader.parse(content,root)) {
            dllPath = root["dllPath"].asString();
            strHhKeyVal = root["hhkey"].asString();
            strHlKeyVal = root["hlkey"].asString();
            hhKeys = root["hhkeyRedis"];
            hlKeys = root["hlkeyRedis"];
            machineStatusRoot = root["machineStatus"];

            HlKeysMap["toolNo"] = hlKeys["toolNo"].asString();
            HlKeysMap["programName"] = hlKeys["programName"].asString();
            HlKeysMap["programStartTime"] = hlKeys["programStartTime"].asString();
            HlKeysMap["programEndTime"] = hlKeys["programEndTime"].asString();
            HlKeysMap["countStatus"] = hlKeys["countStatus"].asString();
            HlKeysMap["mahineStatusKey"] = hlKeys["mahineStatusKey"].asString();

            HhKeysMap["load"] = hhKeys["load"].asString();

            machineStatusMap["work"] = machineStatusRoot["work"].asString();
            machineStatusMap["free"] = machineStatusRoot["free"].asString();
            machineStatusMap["hold"] = machineStatusRoot["hold"].asString();
        }
    }

    map<string,string>::iterator it;
    for (it = HhKeysMap.begin();it!=HhKeysMap.end();++it) {
        cout<<it->first<<" "<<it->second<<endl;
    }

    cout<<"HlKeyMap:"<<endl;
    map<string,string>::iterator it1;
    for (it1 = HlKeysMap.begin();it1!=HlKeysMap.end();++it1) {
        cout<<it1->first<<" "<<it1->second<<endl;
    }
}

//盒子公共配置，获取盒子machineId
void initCommConfig(string path) {
    Config config;
    if (config.FileExist(path)) {
        config.ReadFile(path);
        machineId = config.Read<string>("machineno","");
        cout<<"***********::"<<machineId<<endl;
    } else {
        cout<<"ini config file not exist!"<<endl;
    }
}


void processMsg(string strContent) {
    Json::Reader reader;
    Json::Value root;
    if (reader.parse(strContent,root)) {
        int iOrder = root["order"].asInt();
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
                    thread thStudyProcess(studyProcess,"study",studyId);
                    thStudyProcess.detach();
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

int main(int argc,char *argv[]) {
    //初始化配置文件
    //machineId配置
    programName = "test.iso";
    initCommConfig("common.properties");
//    //获取阈值配置文件信息
    string tmpToolcontent = getConfigContent("toolVal.json");
    if (tmpToolcontent != "") {
        initLocalConfig(tmpToolcontent);
    }

    //获取
    string tmpToolLifeContent = getConfigContent("toolLife.json");
    if (tmpToolLifeContent != "") {
        initToolConfig(tmpToolLifeContent);
    }

    //初始化算法库
#ifdef WIN32
    if (dllPath != "") {
        HINSTANCE winDll = LoadLibrary(_T("tooldll.dll"));
        if (winDll != NULL) {
            initFun = (pInitFun)GetProcAddress(winDll,"initial");
            feedFun = (pFeedFun)GetProcAddress(winDll,"feed");
            resultFun = (pResultFun)GetProcAddress(winDll,"result");
        }
    }
#else
    void *plib = dlopen("./libtool.so",RTLD_NOW | RTLD_GLOBAL);
    if (!plib) {
        cout<<"error msg:"<<dlerror()<<endl;
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
        resultFun = (pResultFun)dlsym(plib,"result");
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
    redisMap.clear();
    bool redisStatus = false;
    int redisCount = 0;
    while (!redisStatus) {
        redisStatus = redisClient.Connect(redisAddr,redisPort);
        cout<<"redis connect status:"<<redisStatus<<endl;
        redisCount++;
        this_thread::sleep_for(chrono::seconds(1));
        if (redisCount == 10) {
            cout<<"tool local redis connect fail"<<endl;
            redisStatus = true;
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

//启动redis数据采集线程
    thread thCollectData(collectDataProcess);
    thCollectData.detach();

    //生成获取阈值报文消息
    vcSendMsgs.push_back(getValConfigMsg());

    //启动预警信息计算处理
    thread thAlertToolProcess(alertToolProcess,"alarmProcess");
    thAlertToolProcess.detach();

   //处理消息 发送消息
   while (1) {
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
               cout<<"pub msg ok"<<endl;
           }
       }
   }


//
    cout<<"input q to exit"<<endl;
    while (std::tolower(std::cin.get()) != 'q') {
//        bool redisStatus = redisClient.Connect("127.0.0.1",6379);
//        cout<<"redisConn status:"<<redisStatus<<endl;
//        cout<<"redis connetct status:"<<redisClient.CheckStatus()<<endl;
    }

//   try {
//       client.unsubscribe("hello")->wait();
//       client.stop_consuming();
//       client.disconnect()->wait();
//       cout<<"discontent ok"<<endl;
//   } catch (mqtt::exception &exc) {
//       cerr << exc.what() << endl;
//       return 1;
//   }
//    try {
//        redisClient.Disconnect();
//    }catch (exception e) {
//        cerr << e.what()<<endl;
//    }

    return 0;
}