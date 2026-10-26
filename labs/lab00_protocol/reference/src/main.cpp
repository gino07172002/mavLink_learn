#include "commander.hpp"
#include <cstdlib>
namespace {
unsigned checks=0;
void require(bool ok,const char* label) {
    ++checks;
    if(!ok) throw std::runtime_error(std::string("FAIL: ")+label);
    std::cout<<"PASS "<<label<<'\n';
}
}
int main() {return lab::run([] {
    mavlink_message_t heartbeat{};
    mavlink_msg_heartbeat_pack(1,1,&heartbeat,MAV_TYPE_QUADROTOR,MAV_AUTOPILOT_GENERIC,0,0,MAV_STATE_ACTIVE);
    const auto bytes=lab::encode(heartbeat);
    require(bytes[0]==0xfd,"MAVLink 2 magic");
    lab::Parser parser;std::vector<mavlink_message_t> messages;
    auto accept=[&](const mavlink_message_t& msg){messages.push_back(msg);};
    parser.feed(bytes.data(),3,accept);
    require(messages.empty(),"partial header emits no message");
    parser.feed(bytes.data()+3,bytes.size()-3,accept);
    require(messages.size()==1 && messages[0].sysid==1,"split frame decoded");
    std::vector<uint8_t> twice=bytes;twice.insert(twice.end(),bytes.begin(),bytes.end());
    parser.feed(twice.data(),twice.size(),accept);
    require(messages.size()==3,"two frames in one datagram");
    auto bad=bytes;bad[10]^=0x01;
    parser.feed(bad.data(),bad.size(),accept);
    require(messages.size()==3 && parser.bad_frames==1,"bad CRC rejected");
    const uint8_t junk[]={0x00,0x01,0xaa};parser.feed(junk,sizeof(junk),accept);
    parser.feed(bytes.data(),bytes.size(),accept);
    require(messages.size()==4,"resynchronizes after corruption and noise");
    lab::Parser independent;unsigned independent_count=0;
    parser.feed(bytes.data(),5,accept);
    independent.feed(bytes.data(),bytes.size(),[&](const mavlink_message_t&){++independent_count;});
    parser.feed(bytes.data()+5,bytes.size()-5,accept);
    require(independent_count==1 && messages.size()==5,"parser instances do not share partial state");
    mavlink_message_t ack_msg{};mavlink_command_ack_t ack{};
    ack_msg.sysid=1;ack_msg.compid=1;ack.command=MAV_CMD_COMPONENT_ARM_DISARM;
    require(lab::ack_matches(ack_msg,ack,1,1,ack.command),"ACK without extension targets accepted");
    ack.target_system=42;
    require(!lab::ack_matches(ack_msg,ack,1,1,ack.command),"ACK addressed to another client rejected");
    ack.target_system=lab::client_sys;ack.target_component=lab::client_comp;
    require(lab::ack_matches(ack_msg,ack,1,1,ack.command),"ACK matching client accepted");
    ack_msg.sysid=2;
    require(!lab::ack_matches(ack_msg,ack,1,1,ack.command),"ACK from another vehicle rejected");
    ack_msg.sysid=1;ack_msg.compid=2;
    require(!lab::ack_matches(ack_msg,ack,1,1,ack.command),"ACK from another component rejected");
    ack_msg.compid=1;
    require(!lab::ack_matches(ack_msg,ack,1,1,MAV_CMD_NAV_TAKEOFF),"ACK for another command rejected");
    require(lab::position_mask==2552,"position+yaw mask is 2552");
    lab::Commander commander;auto t=lab::Clock::now();
    lab::VehicleCommand mode{MAV_CMD_DO_SET_MODE,{1,6,0,0,0,0,0}};
    require(commander.command(mode,t)==MAV_RESULT_TEMPORARILY_REJECTED,"Offboard rejected without prestream");
    mavlink_set_position_target_local_ned_t sp{};
    sp.coordinate_frame=MAV_FRAME_LOCAL_NED;sp.type_mask=lab::position_mask;sp.x=2;sp.z=-2;
    sp.coordinate_frame=MAV_FRAME_BODY_NED;
    require(!commander.setpoint(sp,t),"unsupported frame rejected");
    sp.coordinate_frame=MAV_FRAME_LOCAL_NED;sp.type_mask=0;
    require(!commander.setpoint(sp,t),"unsupported mask rejected");
    sp.type_mask=lab::position_mask;sp.x=std::nanf("");
    require(!commander.setpoint(sp,t),"NaN setpoint rejected");sp.x=2;
    for(int i=0;i<=24;++i) commander.setpoint(sp,t+std::chrono::milliseconds(50*i));
    t+=std::chrono::milliseconds(1200);
    require(commander.command(mode,t)==MAV_RESULT_ACCEPTED && commander.offboard,"Offboard accepted after prestream");
    lab::VehicleCommand arm{MAV_CMD_COMPONENT_ARM_DISARM,{1,0,0,0,0,0,0}};
    require(commander.command(arm,t)==MAV_RESULT_ACCEPTED && commander.armed,"Commander arms");
    auto count=commander.applied;commander.command(arm,t);
    require(commander.applied==count,"repeated arm is idempotent");
    commander.tick(t,0.1f);
    require(commander.x>0 && commander.z<0,"NED kinematic response");
    require(!commander.tick(t+std::chrono::milliseconds(690),0),"no premature failsafe");
    require(commander.tick(t+std::chrono::milliseconds(710),0) && !commander.offboard,"setpoint loss leaves Offboard");
    commander.deny_arm=true;
    require(commander.command(arm,t)==MAV_RESULT_DENIED,"arm denial reported");
    lab::VehicleCommand unknown{65500,{}};
    require(commander.command(unknown,t)==MAV_RESULT_UNSUPPORTED,"unknown command unsupported");
    std::cout<<"CHECKS="<<checks<<'\n';return 0;
});}
