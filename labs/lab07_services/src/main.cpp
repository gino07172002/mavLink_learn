#include "lab.hpp"
#include <cstring>
#include <map>

// This exercise changes one FLOAT parameter and an initially empty mission in a private SITL.
// The runner creates that private PX4 instance. No flight is initiated by this executable.
int main(int argc,char** argv) {return lab::run([&] {
    if(argc<2 || std::string(argv[1])!="--sitl")
        throw std::runtime_error("usage: lab07_services --sitl [local_port 127.0.0.1 peer_port]");
    lab::Args args;args.local=14650;args.remote=14660;args.parse(argc,argv,2);
    if(args.ip!="127.0.0.1") throw std::runtime_error("private localhost SITL required");
    lab::Client client(args);
    if(!client.discover(8) || client.heartbeat.autopilot!=MAV_AUTOPILOT_PX4 || client.armed())
        throw std::runtime_error("Expected disarmed PX4 SITL");
    std::map<std::string,mavlink_param_value_t> parameters;
    std::optional<mavlink_timesync_t> sync;
    std::optional<mavlink_mission_count_t> count;
    std::optional<mavlink_mission_request_int_t> request;
    std::optional<mavlink_mission_item_int_t> item;
    std::optional<mavlink_mission_ack_t> ack;
    auto addressed=[](uint8_t sys,uint8_t comp) {
        return (sys==0 || sys==lab::client_sys) && (comp==0 || comp==lab::client_comp);
    };
    client.observer=[&](const mavlink_message_t& msg) {
        if(msg.msgid==MAVLINK_MSG_ID_PARAM_VALUE) {
            mavlink_param_value_t p{};mavlink_msg_param_value_decode(&msg,&p);
            parameters[std::string(p.param_id,std::find(p.param_id,p.param_id+16,'\0'))]=p;
        }
        if(msg.msgid==MAVLINK_MSG_ID_TIMESYNC) {
            mavlink_timesync_t s{};mavlink_msg_timesync_decode(&msg,&s);
            if(s.tc1 && addressed(s.target_system,s.target_component)) sync=s;
        }
        if(msg.msgid==MAVLINK_MSG_ID_MISSION_COUNT) {
            mavlink_mission_count_t m{};mavlink_msg_mission_count_decode(&msg,&m);
            if(m.mission_type==MAV_MISSION_TYPE_MISSION && addressed(m.target_system,m.target_component)) count=m;
        }
        if(msg.msgid==MAVLINK_MSG_ID_MISSION_REQUEST_INT) {
            mavlink_mission_request_int_t m{};mavlink_msg_mission_request_int_decode(&msg,&m);
            if(m.mission_type==MAV_MISSION_TYPE_MISSION && addressed(m.target_system,m.target_component)) request=m;
        }
        if(msg.msgid==MAVLINK_MSG_ID_MISSION_ITEM_INT) {
            mavlink_mission_item_int_t m{};mavlink_msg_mission_item_int_decode(&msg,&m);
            if(m.mission_type==MAV_MISSION_TYPE_MISSION && addressed(m.target_system,m.target_component)) item=m;
        }
        if(msg.msgid==MAVLINK_MSG_ID_MISSION_ACK) {
            mavlink_mission_ack_t m{};mavlink_msg_mission_ack_decode(&msg,&m);
            if(m.mission_type==MAV_MISSION_TYPE_MISSION && addressed(m.target_system,m.target_component)) ack=m;
        }
    };
    auto wait=[&](auto ready,auto send) {
        for(int attempt=0;attempt<3;++attempt) {
            send();auto begin=lab::Clock::now();
            while(lab::seconds(lab::Clock::now(),begin)<1.5) {client.poll();if(ready()) return;}
        }
        throw std::runtime_error("Microservice response timeout");
    };
    auto read_param=[&](const char* name) {
        parameters.erase(name);mavlink_param_request_read_t p{};
        p.target_system=client.sys;p.target_component=client.comp;p.param_index=-1;
        std::memcpy(p.param_id,name,std::min<size_t>(16,std::strlen(name)));
        wait([&]{return parameters.count(name)>0;},[&]{mavlink_message_t m{};
            mavlink_msg_param_request_read_encode(lab::client_sys,lab::client_comp,&m,&p);client.send(m);});
        return parameters.at(name);
    };
    auto set_float=[&](const char* name,float value) {
        parameters.erase(name);mavlink_param_set_t p{};p.target_system=client.sys;p.target_component=client.comp;
        p.param_type=MAV_PARAM_TYPE_REAL32;p.param_value=value;std::memcpy(p.param_id,name,std::strlen(name));
        wait([&]{return parameters.count(name)>0 && parameters.at(name).param_type==MAV_PARAM_TYPE_REAL32 &&
                         std::abs(parameters.at(name).param_value-value)<0.001f;},
             [&]{mavlink_message_t m{};mavlink_msg_param_set_encode(lab::client_sys,lab::client_comp,&m,&p);client.send(m);});
        if(std::abs(read_param(name).param_value-value)>0.001f) throw std::runtime_error("Parameter readback mismatch");
    };
    const auto autostart=read_param("SYS_AUTOSTART");
    if(autostart.param_type!=MAV_PARAM_TYPE_INT32) throw std::runtime_error("Unexpected SYS_AUTOSTART type");
    // PX4's documented bytewise encoding. Do not numerically cast this wire float to int.
    int32_t autostart_id=0;std::memcpy(&autostart_id,&autostart.param_value,sizeof(autostart_id));
    if(autostart_id!=10040) throw std::runtime_error("Expected SIH quad-X airframe 10040");
    std::cout<<"PARAM_INT SYS_AUTOSTART="<<autostart_id<<'\n';
    const auto original=read_param("COM_OF_LOSS_T");
    if(original.param_type!=MAV_PARAM_TYPE_REAL32) throw std::runtime_error("Unexpected timeout parameter type");
    try {set_float("COM_OF_LOSS_T",0.8f);set_float("COM_OF_LOSS_T",original.param_value);}
    catch(...) {try {set_float("COM_OF_LOSS_T",original.param_value);} catch(...) {}throw;}
    std::cout<<"PARAM_WRITE_READ_RESTORE original="<<original.param_value<<'\n';

    std::vector<double> offsets;double max_rtt=0;
    for(int sample=0;sample<8;++sample) {
        sync.reset();mavlink_timesync_t s{};
        s.ts1=std::chrono::duration_cast<std::chrono::nanoseconds>(lab::Clock::now().time_since_epoch()).count();
        s.target_system=client.sys;s.target_component=client.comp;
        wait([&]{return sync && sync->ts1==s.ts1;},[&]{mavlink_message_t m{};
            mavlink_msg_timesync_encode(lab::client_sys,lab::client_comp,&m,&s);client.send(m);});
        const int64_t end=std::chrono::duration_cast<std::chrono::nanoseconds>(lab::Clock::now().time_since_epoch()).count();
        const double rtt=(end-s.ts1)/1e6;max_rtt=std::max(max_rtt,rtt);
        offsets.push_back((sync->tc1-(s.ts1+(end-s.ts1)/2))/1e6);
    }
    std::sort(offsets.begin(),offsets.end());
    std::cout<<"TIMESYNC samples=8 median_offset_ms="<<offsets[offsets.size()/2]<<" max_rtt_ms="<<max_rtt<<'\n';

    auto finish_download=[&] {
        mavlink_mission_ack_t a{};a.target_system=client.sys;a.target_component=client.comp;
        a.type=MAV_MISSION_ACCEPTED;a.mission_type=MAV_MISSION_TYPE_MISSION;
        mavlink_message_t m{};mavlink_msg_mission_ack_encode(lab::client_sys,lab::client_comp,&m,&a);client.send(m);
    };
    auto read_count=[&] {
        count.reset();mavlink_mission_request_list_t r{};r.target_system=client.sys;r.target_component=client.comp;
        wait([&]{return count.has_value();},[&]{mavlink_message_t m{};
            mavlink_msg_mission_request_list_encode(lab::client_sys,lab::client_comp,&m,&r);client.send(m);});
        return count->count;
    };
    if(read_count()!=0) throw std::runtime_error("Mission must initially be empty; refusing to replace an existing plan");
    finish_download();
    std::array<mavlink_mission_item_int_t,3> plan{};
    for(size_t i=0;i<plan.size();++i) {
        auto& p=plan[i];p.target_system=client.sys;p.target_component=client.comp;p.seq=static_cast<uint16_t>(i);
        p.frame=MAV_FRAME_GLOBAL_RELATIVE_ALT_INT;p.current=i==0;p.autocontinue=1;
        p.x=473977420+(i==1?180:0);p.y=85455940;p.z=i==2?0.0f:2.0f;
        p.command=i==0?MAV_CMD_NAV_TAKEOFF:(i==1?MAV_CMD_NAV_WAYPOINT:MAV_CMD_NAV_LAND);
        p.param2=i==1?0.5f:0.0f;
    }
    request.reset();ack.reset();mavlink_mission_count_t upload{};
    upload.target_system=client.sys;upload.target_component=client.comp;upload.count=3;
    wait([&]{return request.has_value();},[&]{mavlink_message_t m{};
        mavlink_msg_mission_count_encode(lab::client_sys,lab::client_comp,&m,&upload);client.send(m);});
    auto deadline=lab::Clock::now()+std::chrono::seconds(15);
    while(!ack && lab::Clock::now()<deadline) {
        if(request) {
            if(request->seq>=plan.size()) throw std::runtime_error("Unexpected mission item request");
            mavlink_message_t m{};mavlink_msg_mission_item_int_encode(lab::client_sys,lab::client_comp,&m,&plan[request->seq]);
            client.send(m);request.reset();
        }
        client.poll();
    }
    if(!ack || ack->type!=MAV_MISSION_ACCEPTED)
        throw std::runtime_error("Mission upload result="+(ack?std::to_string(ack->type):"timeout"));
    if(read_count()!=plan.size()) throw std::runtime_error("Mission count mismatch");
    for(uint16_t i=0;i<plan.size();++i) {
        item.reset();mavlink_mission_request_int_t r{};r.target_system=client.sys;r.target_component=client.comp;r.seq=i;
        wait([&]{return item && item->seq==i;},[&]{mavlink_message_t m{};
            mavlink_msg_mission_request_int_encode(lab::client_sys,lab::client_comp,&m,&r);client.send(m);});
        if(item->command!=plan[i].command || item->x!=plan[i].x || item->y!=plan[i].y ||
           std::abs(item->z-plan[i].z)>0.001f) throw std::runtime_error("Mission item readback mismatch");
    }
    finish_download();ack.reset();mavlink_mission_clear_all_t clear{};
    clear.target_system=client.sys;clear.target_component=client.comp;
    wait([&]{return ack && ack->type==MAV_MISSION_ACCEPTED;},[&]{mavlink_message_t m{};
        mavlink_msg_mission_clear_all_encode(lab::client_sys,lab::client_comp,&m,&clear);client.send(m);});
    if(read_count()!=0) throw std::runtime_error("Mission clear unconfirmed");
    finish_download();
    std::cout<<"MISSION_UPLOAD_DOWNLOAD_CLEAR items=3 (not executed)\nSERVICES_PASS\n";return 0;
});}
