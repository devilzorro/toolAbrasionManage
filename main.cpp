#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <cctype>
#include <thread>
#include <chrono>
#include <atomic>
#include "json/json.h"
#include "redisClient/redisClient.h"

#ifdef WIN32
#include "mqtt-win/async_client.h"
#include <Windows.h>
#pragma comment(lib,"ws2_32.lib")
#else
#include <mqtt/async_client.h>
#include <dlfcn.h>
#endif

using namespace std;

const string LOCALADDR {"tcp://localhost:1883"};

const string TOPIC {"hello"};

const string CLIENT_ID {"TESTXXX"};

const string SERVER_ADDRESS {"testxxxxxx"};

const int N_RETRY_ATTEMPTS = 5;
//const int QOS = 1;

static bool mqttStatus = false;

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
    }
};


void processMsg(string passData) {
    cout<<"pass data :"<<passData<<endl;
    while (1)
    {
        cout<<"thread running"<<endl;
        this_thread::sleep_for(chrono::milliseconds(1000));
        //操作 ;
    }
}

void processData() {

}

void initConfigFile(string filePth) {

}

int main(int argc,char *argv[]) {
    string strJson = "{\"uploadid\": \"UP000000\",\"code\": 100,\"msg\": \"\",\"files\": \"\"}";
    Json::Reader reader;
    Json::Value root;
    if (reader.parse(strJson,root)) {
        string id = root["uploadid"].asString();
        cout<<"json reader:"<<id;
    } else {
        return 1;
    }

     CRedisClient redisClient;

    redisClient.Connect("localhost",6379);

    //子线程初始化
    string strTest = "hello world!";
    thread tProcessMsg(processMsg,strTest);
    tProcessMsg.detach();


    //mqtt broker 连接
    mqtt::async_client client("tcp://localhost:1883","test1");
    mqtt::connect_options opts;
//    mqtt::string_ref usrname
    opts.set_keep_alive_interval(20);
    opts.set_clean_session(true);
//    opts.set_user_name("admin");
//    opts.set_password("password");
    callback cb;
    client.set_callback(cb);
    cout<<"mqtt init finish********"<<endl;


   try {
       cout << "\nConnecting..." << endl;
       mqtt::token_ptr conntok = client.connect(opts);
       cout << "Waiting for the connection..." << endl;
       conntok->wait();
       client.start_consuming();
       client.subscribe("hello",1)->wait();
       cout<<"subscr topic hello ok!"<<endl;


       cout << "  ...OK" << endl;

       cout<<"callback member conn status:"<<cb.bConnStatus<<endl;

       mqtt::message_ptr msg = mqtt::make_message("hello","msg content1");
       msg->set_qos(0);
       client.publish(msg)->wait_for(10);
       cout<<"pub msg ok"<<endl;
   } catch (const mqtt::exception &exc) {
       cerr << exc.what() << endl;
       return 1;
   }
//
//    cout<<"input q to quit"<<endl;
////    while (std::tolower(std::cin.get()) != 'x') {
////    }
//
    while (std::tolower(std::cin.get()) != 'q') {
        cout<<"redis connetct status:"<<redisClient.CheckStatus()<<endl;
    }

   try {
       client.unsubscribe("hello")->wait();
       client.stop_consuming();
       client.disconnect()->wait();
       cout<<"discontent ok"<<endl;
   } catch (mqtt::exception &exc) {
       cerr << exc.what() << endl;
       return 1;
   }
    try {
        redisClient.Disconnect();
    }catch (exception e) {
        cerr << e.what()<<endl;
    }


    return 0;
}