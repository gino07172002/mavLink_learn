#include "lab.hpp"
int main(int argc,char** argv) {return lab::run([&] {
    lab::Args args;args.parse(argc,argv,1);lab::Client client(args);
    if(!client.discover()) return 2;
    // This exercise deliberately stops setpoints to test a toy failsafe; use only our bridge.
    if(client.heartbeat.autopilot!=MAV_AUTOPILOT_GENERIC || client.sys!=1) {
        std::cerr<<"This loss-of-control demo is for lab04_bridge only; see SITL checklist.\n";return 5;
    }
    float x=0;auto next=lab::Clock::now();
    auto stream=[&] {
        auto now=lab::Clock::now();
        if(now>=next) {client.setpoint(x,0,-2);next=now+std::chrono::milliseconds(50);}
    };
    auto begin=lab::Clock::now();
    while(lab::seconds(lab::Clock::now(),begin)<1.3) {stream();client.poll();}
    std::array<float,7> params{};
    params[0]=MAV_MODE_FLAG_CUSTOM_MODE_ENABLED;params[1]=6;
    if(!client.command(MAV_CMD_DO_SET_MODE,params,stream)) return 3;
    if(!client.confirm([&]{return client.offboard();},2.5,stream)) return 4;
    params={};params[0]=1;
    if(!client.command(MAV_CMD_COMPONENT_ARM_DISARM,params,stream)) return 3;
    if(!client.confirm([&]{return client.armed();},2.5,stream)) return 4;
    x=2;begin=lab::Clock::now();bool reached=false;
    while(lab::seconds(lab::Clock::now(),begin)<args.duration) {
        stream();client.poll();
        if(!client.healthy()) return 6;
        if(client.last_position && lab::seconds(lab::Clock::now(),*client.last_position)<0.5 &&
           std::abs(client.position.x-2)<0.1f && std::abs(client.position.z+2)<0.1f) reached=true;
    }
    std::cout<<"POSITION_REACHED="<<reached<<" x="<<client.position.x<<" z="<<client.position.z<<'\n';
    std::cout<<"STOP_SETPOINTS (client heartbeat continues)\n";
    if(!client.confirm([&]{return !client.offboard();},3.0)) return 4;
    std::cout<<"OFFBOARD_LOSS_CONFIRMED\n";
    params={};
    if(!client.command(MAV_CMD_COMPONENT_ARM_DISARM,params)) return 3;
    if(!client.confirm([&]{return !client.armed();})) return 4;
    return reached?0:7;
});}
