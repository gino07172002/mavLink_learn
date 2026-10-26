#pragma once
#include "lab.hpp"
namespace lab {
// A tiny stand-in for vehicle_command -> Commander -> vehicle_command_ack.
// No uORB or PX4 code is linked here. Inputs/outputs are deliberately separate from UDP.
struct VehicleCommand {uint16_t id;std::array<float,7> params;};
class Commander {
    std::optional<Time> first_setpoint_,last_setpoint_;
public:
    bool armed=false,offboard=false,deny_arm=false;
    unsigned applied=0;
    float x=0,y=0,z=0,target_x=0,target_y=0,target_z=0;
    bool setpoint(const mavlink_set_position_target_local_ned_t& sp,Time now) {
        if(sp.coordinate_frame!=MAV_FRAME_LOCAL_NED || sp.type_mask!=position_mask ||
           !std::isfinite(sp.x)||!std::isfinite(sp.y)||!std::isfinite(sp.z)||!std::isfinite(sp.yaw))
            return false;
        if(!last_setpoint_ || seconds(now,*last_setpoint_)>0.5) first_setpoint_=now;
        last_setpoint_=now;target_x=sp.x;target_y=sp.y;target_z=sp.z;return true;
    }
    uint8_t command(const VehicleCommand& cmd,Time now) {
        if(cmd.id==MAV_CMD_COMPONENT_ARM_DISARM) {
            if(cmd.params[0]!=0 && cmd.params[0]!=1) return MAV_RESULT_DENIED;
            if(cmd.params[0]==1 && deny_arm) return MAV_RESULT_DENIED;
            const bool next=cmd.params[0]==1;
            if(armed!=next) {armed=next;++applied;}
            return MAV_RESULT_ACCEPTED;
        }
        if(cmd.id==MAV_CMD_DO_SET_MODE) {
            if(cmd.params[0]!=MAV_MODE_FLAG_CUSTOM_MODE_ENABLED || cmd.params[1]!=6)
                return MAV_RESULT_UNSUPPORTED;
            if(!first_setpoint_ || !last_setpoint_ || seconds(now,*first_setpoint_)<1.0 ||
               seconds(now,*last_setpoint_)>0.5) return MAV_RESULT_TEMPORARILY_REJECTED;
            if(!offboard) {offboard=true;++applied;}return MAV_RESULT_ACCEPTED;
        }
        return MAV_RESULT_UNSUPPORTED;
    }
    bool tick(Time now,float dt) {
        bool lost=offboard && (!last_setpoint_ || seconds(now,*last_setpoint_)>0.7);
        if(lost) offboard=false; // Toy policy: hold last position, NOT a landing implementation.
        if(offboard && armed) {
            const float alpha=std::min(1.0f,3*dt);
            x+=(target_x-x)*alpha;y+=(target_y-y)*alpha;z+=(target_z-z)*alpha;
        }
        return lost;
    }
};
}
