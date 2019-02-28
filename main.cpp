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
//#include "config/config.h"
#include <vector>
#include <time.h>
#include <map>

#ifdef WIN32
#include "mqtt-win/async_client.h"
#include <Windows.h>
#include "win_stdafx/stdafx.h"
#pragma comment(lib,"ws2_32.lib")
#else
#include <mqtt/async_client.h>
#include <dlfcn.h>
#endif

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
string processStatus = "";

//程序名
string programName = "";

string dllPath = "tooldll.dll";
string redisAddr = "127.0.0.1";
int redisPort = 6379;
string mqttAddr = "127.0.0.1";
string strMqttPort = "1883";

vector<string> vcRedisKeys;
vector<string> vcRecvMsgs;
vector<string> vcSendMsgs;

map<string,string> redisMap;
map<int,double> maxLimMap;
map<int,double> maxSensMap;
map<int,double> minLimMap;
map<int,double> minSensMap;
vector<int> vcNoAlarmTool;
map<int,double> alertMap;

CRedisClient redisClient;


struct Results {
    double *r;
    int n;
};

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

void testToolDll() {
    double *p;
    double *r;
    int n;
    Results re;
    double temp;
    double toolnum;
    double load;

    ifstream f;
    f.open("/home/llj/workspace/toolAbrasionManage/cmake-build-release/rawdata.txt");

    int countNum = 0;
    int check = 0;
    double sum = 0;
    cout<<"init fun"<<endl;
    if (initFun != NULL) {
        p = initFun();
    } else {
        cout<<"func pointer NULL"<<endl;
    }

    cout<<"init finish"<<endl;
    for (int i = 0; i < 4000; i++) {
        for (int j = 0; j < 16; j++) {
            f >> temp;
//            cout << "temp content:" << temp;
            if (j == 2) {
                toolnum = temp;
                int tmpNum = temp;
                if(tmpNum == 168) {
                    countNum++;
                    cout<<"toolnum:"<<toolnum<<endl;
                }
            }
            else if (j == 3)
            {
                load = temp;
                if(check != countNum) {
                    check = countNum;
                    sum = sum + temp;
                }

            }
        }
//        cout << endl;

        feedFun(toolnum,load,p);
    }
    f.close();

    double res = sum/countNum;
    cout<<"******test result:"<<res<<endl;

    re = resultFun(p);
    printf("******win libtool run success*******\n");
    r = re.r;
    n = re.n;
    for (int i = 0; i < n; ++i) {
        cout<<"toolNUm:"<<r[i*2]<<endl;
        cout<<"resultValue:"<<r[i*2+1]<<endl;
    }
}

void collectDataProcess() {
    cout<<"collect data thread running"<<endl;
    string tmpProgramName;
    while (1) {
        this_thread::sleep_for(chrono::milliseconds(10));
        //操作
        if (!vcRedisKeys.empty()) {
            for (vector<string>::iterator it = vcRedisKeys.begin();it< vcRedisKeys.end();it++) {
                string key = (*it);
                string val;
                if (redisClient.Get(key,val)) {
                    redisMap[key] = val;
                }
            }
        }

//        if (tmpProgramName != programName) {
//
//        }
    }


//
//    cout<<"redis client:"<<redisClient.CheckStatus()<<endl;
}

Results processToolVal(string flag) {
    double *tmpPointer;
    double *tmpR;
    Results re;
    tmpPointer = initFun();
    while (1) {
        this_thread::sleep_for(chrono::milliseconds(10));
        if (redisMap.empty()) {
            if (redisMap.count("startFlag")) {
                if (redisMap["startFlag"] == "start") {
                    if (flag == "study") {
                        if (studyStatus == "abort") {
                            break;
                        }
                    }
                    //获取redis中存储的数据
                    if (redisMap.count("toolNo")&&redisMap.count("axisLoad")) {
                        stringstream ss;
                        int toolNo;
                        double tmpload;
                        ss<<redisMap["toolNo"];
                        ss>>toolNo;
                        ss<<redisMap["axisLoad"];
                        ss>>tmpload;
                        feedFun(toolNo,tmpload,tmpPointer);
                    }
                }
            }
        }

    }
    re = resultFun(tmpPointer);
    delete tmpPointer;
    tmpPointer = NULL;
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

        dataRoot["fileName"] = programName;
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
                    alarmContent["detail"] = "超过上限";
                    alarmDetailRoot.append(alarmContent);
                }
            }

            if ((!minLimMap.empty())&&(minLimMap.count(toolNo))) {
                if (val < (minLimMap[toolNo]*minSensMap[toolNo])) {
                    bAlarm = true;
                    Json::Value alarmContent;
                    alarmContent["toolNo"] = toolNo;
                    alarmContent["detail"] = "超过下限";
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
        cout<<"error open config file"<<endl;
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
        Json::Reader reader;
        Json::Value value;
        if (reader.parse(content,value)) {

        } else {

        }
    }
}

//盒子公共配置，获取盒子machineId
void initCommConfig(string path) {
//    Config config;
//    if (config.FileExist(path)) {
//        config.ReadFile(path);
//        machineId = config.Read<string>("machineno","");
//        cout<<"***********::"<<machineId<<endl;
//    } else {
//        cout<<"ini config file not exist!"<<endl;
//    }
}


void processMsg(string strContent) {
    Json::Reader reader;
    Json::Value root;
    if (reader.parse(strContent,root)) {
        int iOrder = root["order"].asInt();
        Json::Value contentRoot = root["content"];
        Json::Value dataRoot = contentRoot["data"];
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
                if (studyStatus == "start") {
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
//    initCommConfig("common.properties");
//    //获取阈值配置文件信息
//    string tmpToolcontent = getConfigContent("toolLife.json");
//    if (tmpToolcontent != "") {
//        initLocalConfig(tmpToolcontent);
//    }

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
    void *plib = dlopen("/home/llj/workspace/toolAbrasionManage/cmake-build-release/libtool.so",RTLD_NOW | RTLD_GLOBAL);
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
    int tmpStatus = dlclose(plib);
    cout<<"dl close stastus:"<<tmpStatus<<endl;
    cout<<"dlopen finish"<<endl;
#endif


    cout<<"test thread start"<<endl;
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
       cout<<"subscr topic hello ok!"<<endl;
       cout << "  ...OK" << endl;
       cout<<"callback member conn status:"<<cb.bConnStatus<<endl;

        //mqtt发送消息
//       mqtt::message_ptr msg = mqtt::make_message("hello","msg content1");
//       msg->set_qos(0);
//       client.publish(msg)->wait_for(2);
//       cout<<"pub msg ok"<<endl;
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

   //linux动态库测试
//    thread thTest(testToolDll);
//    thTest.detach();

//启动redis数据采集线程
//    thread thCollectData(collectDataProcess);
//    thCollectData.detach();

    //生成获取阈值报文消息
    vcSendMsgs.push_back(getValConfigMsg());

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


    cout<<"input x to test local redis connect"<<endl;
    while (std::tolower(std::cin.get()) != 'x') {

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