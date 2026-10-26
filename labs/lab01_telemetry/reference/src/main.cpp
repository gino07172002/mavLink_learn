#include "lab.hpp"
int main(int argc,char** argv) {return lab::run([&] {
    lab::Args args;args.parse(argc,argv,1);lab::Client client(args);
    unsigned attitudes=0;
    client.observer=[&](const mavlink_message_t& msg) {
        if(msg.msgid==MAVLINK_MSG_ID_HEARTBEAT)
            std::cout<<"HEARTBEAT sys="<<int(msg.sysid)<<" comp="<<int(msg.compid)
                <<" armed="<<client.armed()<<'\n';
        if(msg.msgid==MAVLINK_MSG_ID_ATTITUDE) {
            mavlink_attitude_t a{};mavlink_msg_attitude_decode(&msg,&a);++attitudes;
            constexpr double degrees=180.0/3.141592653589793;
            std::cout<<"ATTITUDE deg roll="<<a.roll*degrees<<" pitch="<<a.pitch*degrees
                <<" yaw="<<a.yaw*degrees<<'\n';
        }
    };
    if(!client.discover()) return 2;
    auto begin=lab::Clock::now();
    while(lab::seconds(lab::Clock::now(),begin)<args.duration) {
        client.poll();
        if(!client.healthy()) {std::cerr<<"HEARTBEAT_TIMEOUT\n";return 3;}
    }
    std::cout<<"SUMMARY attitudes="<<attitudes<<'\n';return attitudes?0:4;
});}
