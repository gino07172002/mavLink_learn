#include "commander.hpp"
int main(int argc,char** argv) {return lab::run([&] {
    // Modes are deterministic failure injection for the integration exercises.
    std::string mode=argc>1?argv[1]:"normal";
    const std::vector<std::string> modes={"normal","deny","drop-first","silent-ack","no-state","progress","spoof"};
    if(std::find(modes.begin(),modes.end(),mode)==modes.end()) throw std::runtime_error("unknown bridge mode");
    lab::Args args;args.local=14560;args.remote=14550;args.duration=30;args.parse(argc,argv,2);
    lab::Udp udp(args.local,args.ip,args.remote);lab::Commander commander;commander.deny_arm=mode=="deny";
    auto start=lab::Clock::now(),previous=start,next_hb=start,next_data=start;
    unsigned commands=0;
    std::optional<lab::Time> final_due;
    uint16_t final_command=0;uint8_t final_sys=0,final_comp=0;
    auto ack=[&](uint8_t sender_sys,uint8_t sender_comp,uint16_t command,uint8_t result,
                 uint8_t target_sys,uint8_t target_comp) {
        mavlink_message_t msg{};
        mavlink_msg_command_ack_pack(sender_sys,sender_comp,&msg,command,result,
            result==MAV_RESULT_IN_PROGRESS?50:0,0,target_sys,target_comp);udp.send(msg);
    };
    std::cout<<"BRIDGE_READY port="<<args.local<<" mode="<<mode<<'\n';
    while(lab::seconds(lab::Clock::now(),start)<args.duration) {
        udp.receive(10,[&](const mavlink_message_t& msg) {
            if(msg.msgid==MAVLINK_MSG_ID_SET_POSITION_TARGET_LOCAL_NED) {
                mavlink_set_position_target_local_ned_t sp{};
                mavlink_msg_set_position_target_local_ned_decode(&msg,&sp);
                if(sp.target_system==1 && sp.target_component==1) commander.setpoint(sp,lab::Clock::now());
            }
            if(msg.msgid!=MAVLINK_MSG_ID_COMMAND_LONG) return;
            mavlink_command_long_t cmd{};mavlink_msg_command_long_decode(&msg,&cmd);
            if(cmd.target_system!=1 || cmd.target_component!=1) return;
            ++commands;
            std::cout<<"VEHICLE_COMMAND id="<<cmd.command<<" confirmation="<<int(cmd.confirmation)<<'\n';
            uint8_t result=MAV_RESULT_ACCEPTED;
            if(mode!="no-state") result=commander.command({cmd.command,{cmd.param1,cmd.param2,cmd.param3,
                cmd.param4,cmd.param5,cmd.param6,cmd.param7}},lab::Clock::now());
            if(mode=="silent-ack" || (mode=="drop-first" && commands==1)) return;
            if(mode=="spoof") {
                ack(2,1,cmd.command,MAV_RESULT_ACCEPTED,msg.sysid,msg.compid);
                ack(1,1,cmd.command,MAV_RESULT_ACCEPTED,42,msg.compid);
                ack(1,1,MAV_CMD_NAV_TAKEOFF,MAV_RESULT_ACCEPTED,msg.sysid,msg.compid);
                return;
            }
            if(mode=="progress") {
                ack(1,1,cmd.command,MAV_RESULT_IN_PROGRESS,msg.sysid,msg.compid);
                final_due=lab::Clock::now()+std::chrono::milliseconds(800);
                final_command=cmd.command;final_sys=msg.sysid;final_comp=msg.compid;return;
            }
            ack(1,1,cmd.command,result,msg.sysid,msg.compid);
        });
        auto now=lab::Clock::now();float dt=static_cast<float>(lab::seconds(now,previous));previous=now;
        if(commander.tick(now,dt)) std::cout<<"OFFBOARD_LOSS -> HOLD (toy policy)\n";
        if(final_due && now>=*final_due) {
            ack(1,1,final_command,MAV_RESULT_ACCEPTED,final_sys,final_comp);final_due.reset();
        }
        if(now>=next_hb) {
            mavlink_message_t msg{};
            mavlink_msg_heartbeat_pack(1,1,&msg,MAV_TYPE_QUADROTOR,MAV_AUTOPILOT_GENERIC,
                MAV_MODE_FLAG_CUSTOM_MODE_ENABLED|(commander.armed?MAV_MODE_FLAG_SAFETY_ARMED:0),
                commander.offboard?lab::px4_offboard:0,MAV_STATE_ACTIVE);
            udp.send(msg);next_hb=now+std::chrono::milliseconds(250);
        }
        if(now>=next_data) {
            mavlink_message_t msg{};float t=static_cast<float>(lab::seconds(now,start));
            mavlink_msg_attitude_pack(1,1,&msg,lab::millis(start),0.1f*std::sin(t),0.05f,0.2f,0,0,0);udp.send(msg);
            mavlink_msg_local_position_ned_pack(1,1,&msg,lab::millis(start),commander.x,commander.y,commander.z,0,0,0);
            udp.send(msg);next_data=now+std::chrono::milliseconds(50);
        }
    }
    std::cout<<"BRIDGE_SUMMARY commands="<<commands<<" applied="<<commander.applied<<'\n';return 0;
});}
