#include "lite_backend.hpp"
#include <deque>
#include <thread>

int main(int argc,char** argv) {return lab::run([&] {
    lab::Args args;args.local=14660;args.remote=14650;args.duration=120;args.parse(argc,argv,1);
    lab::Udp udp(args.local,args.ip,args.remote);lab::LiteBackend backend;
    constexpr uint8_t sys=61,comp=1;
    // FIFO for commands/ACKs: unlike a latest-value topic, consecutive commands cannot overwrite each other.
    struct Envelope {uint8_t sys,comp;uint16_t command;std::array<float,7> params;};
    struct Ack {Envelope request;uint8_t result;};
    std::deque<Envelope> commands;std::deque<Ack> acknowledgements;
    struct StateTopic {px4lite::simulator::VehicleState value;uint64_t generation=0;} state_topic;
    auto start=lab::Clock::now(),tick=start,next_hb=start,next_att=start,next_pos=start,next_landed=start;
    std::chrono::microseconds position_interval{50000},landed_interval{100000};
    State previous=backend.commander_state();
    std::cout<<"PX4LITE_READY sys=61 port="<<args.local<<" backend=original-Commander+SimpleQuadSimulator\n";
    while(lab::seconds(lab::Clock::now(),start)<args.duration) {
        udp.receive(0,[&](const mavlink_message_t& msg) {
            if(msg.msgid==MAVLINK_MSG_ID_SET_POSITION_TARGET_LOCAL_NED) {
                mavlink_set_position_target_local_ned_t sp{};mavlink_msg_set_position_target_local_ned_decode(&msg,&sp);
                if(sp.target_system==sys && sp.target_component==comp) backend.setpoint(sp,lab::Clock::now());
            }
            if(msg.msgid==MAVLINK_MSG_ID_COMMAND_LONG) {
                mavlink_command_long_t c{};mavlink_msg_command_long_decode(&msg,&c);
                if(c.target_system!=sys || c.target_component!=comp) return;
                Envelope e{static_cast<uint8_t>(msg.sysid),msg.compid,c.command,
                    {c.param1,c.param2,c.param3,c.param4,c.param5,c.param6,c.param7}};
                if(commands.size()<8) commands.push_back(e);
                else acknowledgements.push_back({e,MAV_RESULT_TEMPORARILY_REJECTED});
            }
        });
        while(!commands.empty()) {
            const auto e=commands.front();commands.pop_front();uint8_t result=MAV_RESULT_UNSUPPORTED;
            if(e.command==MAV_CMD_SET_MESSAGE_INTERVAL && e.params[1]>=10000 && e.params[1]<=1000000) {
                auto interval=std::chrono::microseconds(static_cast<int64_t>(e.params[1]));
                if(e.params[0]==MAVLINK_MSG_ID_LOCAL_POSITION_NED) {position_interval=interval;result=MAV_RESULT_ACCEPTED;}
                if(e.params[0]==MAVLINK_MSG_ID_EXTENDED_SYS_STATE) {landed_interval=interval;result=MAV_RESULT_ACCEPTED;}
            } else result=backend.command(e.command,e.params,lab::Clock::now());
            acknowledgements.push_back({e,result});
            std::cout<<"COMMAND_TOPIC src="<<int(e.sys)<<':'<<int(e.comp)<<" id="<<e.command
                <<" result="<<lab::result_name(result)<<'\n';
        }
        while(!acknowledgements.empty()) {
            const auto a=acknowledgements.front();acknowledgements.pop_front();mavlink_message_t msg{};
            mavlink_msg_command_ack_pack(sys,comp,&msg,a.request.command,a.result,0,0,a.request.sys,a.request.comp);udp.send(msg);
        }
        const auto now=lab::Clock::now();unsigned catchup=0;
        while(tick<=now && catchup++<20) {
            if(backend.tick(tick)) std::cout<<"OFFBOARD_LOSS original-Commander failsafe\n";
            state_topic.value=backend.state();++state_topic.generation;tick+=std::chrono::milliseconds(4);
        }
        if(catchup>=20 && tick<=now) tick=now+std::chrono::milliseconds(4);
        if(previous!=backend.commander_state()) {
            std::cout<<"COMMANDER_STATE "<<stateToString(previous)<<" -> "<<stateToString(backend.commander_state())<<'\n';
            previous=backend.commander_state();
        }
        const auto& s=state_topic.value;mavlink_message_t msg{};
        if(now>=next_hb) {
            mavlink_msg_heartbeat_pack(sys,comp,&msg,MAV_TYPE_QUADROTOR,MAV_AUTOPILOT_GENERIC,
                MAV_MODE_FLAG_CUSTOM_MODE_ENABLED|(backend.armed()?MAV_MODE_FLAG_SAFETY_ARMED:0),backend.mode(),MAV_STATE_ACTIVE);
            udp.send(msg);next_hb=now+std::chrono::milliseconds(250);
        }
        if(now>=next_att) {
            mavlink_msg_attitude_pack(sys,comp,&msg,lab::millis(start),static_cast<float>(s.eulerRad.rollRad),
                static_cast<float>(s.eulerRad.pitchRad),static_cast<float>(s.eulerRad.yawRad),
                static_cast<float>(s.angularVelocityBodyRadS.x),static_cast<float>(s.angularVelocityBodyRadS.y),
                static_cast<float>(s.angularVelocityBodyRadS.z));udp.send(msg);next_att=now+std::chrono::milliseconds(50);
        }
        if(now>=next_pos) {
            mavlink_msg_local_position_ned_pack(sys,comp,&msg,lab::millis(start),
                static_cast<float>(s.positionNedM.x),static_cast<float>(s.positionNedM.y),static_cast<float>(s.positionNedM.z),
                static_cast<float>(s.velocityNedMps.x),static_cast<float>(s.velocityNedMps.y),static_cast<float>(s.velocityNedMps.z));
            udp.send(msg);next_pos=now+position_interval;
        }
        if(now>=next_landed) {
            mavlink_msg_extended_sys_state_pack(sys,comp,&msg,MAV_VTOL_STATE_UNDEFINED,
                s.onGround?MAV_LANDED_STATE_ON_GROUND:MAV_LANDED_STATE_IN_AIR);udp.send(msg);next_landed=now+landed_interval;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return 0;
});}
