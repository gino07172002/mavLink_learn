#pragma once
#include "lab.hpp"
// These are the user's existing implementations, compiled directly via PX4LITE_ROOT.
#include <lab07/commander.hpp>
#include <lab06/control_stack.hpp>
#include <supplementary/02_position_control/position_control.hpp>
#include <supplementary/04_control_allocator/control_allocator.hpp>
#include <px4lite/simple_quad_simulator.hpp>

namespace lab {
class LiteBackend {
    ::Commander commander_;
    px4lite::simulator::SimpleQuadSimulator simulator_;
    px4lite::supplementary::MulticopterPositionControl position_control_;
    px4lite::lab06::AttitudeController roll_{4},pitch_{4};
    px4lite::lab06::RateController roll_rate_{0.12f,0.015f,0.002f},pitch_rate_{0.12f,0.015f,0.002f};
    const HealthStatus health_{true,true,true};
    std::optional<Time> first_sp_,last_sp_;
    float target_x_=0,target_y_=0,target_z_=0;
    bool offboard_=false,landing_=false;
public:
    LiteBackend() {commander_.init();}
    const auto& state() const {return simulator_.state();}
    auto commander_state() const {return commander_.state();}
    bool armed() const {return commander_.state()!=State::INIT && commander_.state()!=State::STANDBY;}
    bool offboard() const {return offboard_;}
    uint32_t mode() const {return offboard_?px4_offboard:(landing_?((4u<<16)|(6u<<24)):0);}
    bool setpoint(const mavlink_set_position_target_local_ned_t& sp,Time now) {
        if(sp.coordinate_frame!=MAV_FRAME_LOCAL_NED || sp.type_mask!=position_mask ||
           !std::isfinite(sp.x)||!std::isfinite(sp.y)||!std::isfinite(sp.z)||sp.yaw!=0 ||
           std::hypot(sp.x,sp.y)>6 || sp.z>0.5f || sp.z< -4) return false;
        if(!last_sp_ || seconds(now,*last_sp_)>0.5) first_sp_=now;
        last_sp_=now;target_x_=sp.x;target_y_=sp.y;target_z_=sp.z;return true;
    }
    uint8_t command(uint16_t id,const std::array<float,7>& p,Time now) {
        if(id==MAV_CMD_COMPONENT_ARM_DISARM) {
            if(p[0]==1) {
                if(armed()) return MAV_RESULT_ACCEPTED;
                return commander_.arm(health_)?MAV_RESULT_ACCEPTED:MAV_RESULT_DENIED;
            }
            if(p[0]==0) {
                if(!armed()) return MAV_RESULT_ACCEPTED;
                return commander_.disarm()?MAV_RESULT_ACCEPTED:MAV_RESULT_DENIED;
            }
            return MAV_RESULT_DENIED;
        }
        if(id==MAV_CMD_DO_SET_MODE) {
            if(p[0]!=MAV_MODE_FLAG_CUSTOM_MODE_ENABLED) return MAV_RESULT_UNSUPPORTED;
            if(p[1]==6) {
                if(!first_sp_ || !last_sp_ || seconds(now,*first_sp_)<1 || seconds(now,*last_sp_)>0.5)
                    return MAV_RESULT_TEMPORARILY_REJECTED;
                offboard_=true;landing_=false;return MAV_RESULT_ACCEPTED;
            }
            if(p[1]==4 && p[2]==6) {
                if(!landing_ && !commander_.land()) return MAV_RESULT_DENIED;
                target_x_=static_cast<float>(state().positionNedM.x);
                target_y_=static_cast<float>(state().positionNedM.y);
                offboard_=false;landing_=true;return MAV_RESULT_ACCEPTED;
            }
        }
        return MAV_RESULT_UNSUPPORTED;
    }
    // Fixed 250 Hz plant/controller tick. Time argument is only for communication freshness.
    bool tick(Time now) {
        bool lost=offboard_ && last_sp_ && seconds(now,*last_sp_)>0.7;
        if(lost) {
            offboard_=false;landing_=armed();
            target_x_=static_cast<float>(state().positionNedM.x);
            target_y_=static_cast<float>(state().positionNedM.y);
            // Exercise the original Commander's health -> FAILSAFE -> LANDING path.
            if(armed()) commander_.update(VehicleStatus{!state().onGround},HealthStatus{false,true,true});
        }
        if(offboard_ && commander_.state()==State::ARMED) commander_.takeoff(health_);
        const auto& s=state();
        px4lite::simulator::MotorCommands motors{};
        if(armed() && (offboard_ || landing_)) {
            constexpr float dt=0.004f,deg=57.295779513f;
            using px4lite::supplementary::Vector3f;
            const auto sp=position_control_.update(
                Vector3f{target_x_,target_y_,landing_?-0.2f:-target_z_},
                Vector3f{static_cast<float>(s.positionNedM.x),static_cast<float>(s.positionNedM.y),static_cast<float>(-s.positionNedM.z)},
                Vector3f{static_cast<float>(s.velocityNedMps.x),static_cast<float>(s.velocityNedMps.y),static_cast<float>(-s.velocityNedMps.z)},dt);
            const float r=std::clamp(sp.roll_deg/deg,-0.55f,0.55f);
            const float p=std::clamp(sp.pitch_deg/deg,-0.55f,0.55f);
            const float rt=roll_rate_.update(roll_.update(r,static_cast<float>(s.eulerRad.rollRad)),
                                           static_cast<float>(s.angularVelocityBodyRadS.x),dt);
            const float pt=pitch_rate_.update(pitch_.update(p,static_cast<float>(s.eulerRad.pitchRad)),
                                            static_cast<float>(s.angularVelocityBodyRadS.y),dt);
            // Same pitch adapter as the original simulator/demo/lite_flight_demo.cpp.
            const auto output=px4lite::supplementary::PX4ControlAllocator::allocate(rt,-pt,0,sp.thrust_norm);
            for(size_t i=0;i<4;++i) motors.normalized[i]=output.m[i];
        } else {
            position_control_={};roll_rate_.reset();pitch_rate_.reset();
        }
        simulator_.setMotorCommands(motors);simulator_.step();
        commander_.update(VehicleStatus{!state().onGround},health_);
        return lost;
    }
};
}
