#include "lab.hpp"
// Explicitly opt-in, localhost-only simulator exercise. The runner starts the PX4 process.
// Normal flight and deliberate Offboard-loss are separate invocations.
int main(int argc,char** argv) {return lab::run([&] {
    const std::string scenario=argc>1?argv[1]:"";
    if(scenario!="--sitl" && scenario!="--sitl-loss" && scenario!="--lite" && scenario!="--lite-loss")
        throw std::runtime_error("usage: lab05_sitl --sitl|--sitl-loss|--lite|--lite-loss [local_port 127.0.0.1 peer_port phase_timeout]");
    const bool lite=scenario=="--lite" || scenario=="--lite-loss";
    const bool loss=scenario=="--sitl-loss" || scenario=="--lite-loss";
    lab::Args args;args.local=14650;args.remote=14660;args.duration=20;args.parse(argc,argv,2);
    if(args.ip!="127.0.0.1") throw std::runtime_error("SITL exercise requires localhost");
    lab::Client client(args);
    if(!client.discover(8)) return 2;
    if(client.heartbeat.autopilot!=(lite?MAV_AUTOPILOT_GENERIC:MAV_AUTOPILOT_PX4) ||
       client.heartbeat.type!=MAV_TYPE_QUADROTOR || (lite && client.sys!=61))
        throw std::runtime_error("Unexpected simulator identity (PX4-Lite uses sysid 61)");
    for(auto id:{MAVLINK_MSG_ID_LOCAL_POSITION_NED,MAVLINK_MSG_ID_EXTENDED_SYS_STATE}) {
        std::array<float,7> p{};p[0]=static_cast<float>(id);p[1]=100000;
        if(!client.command(MAV_CMD_SET_MESSAGE_INTERVAL,p))
            throw std::runtime_error("Required stream request failed");
    }
    auto begin=lab::Clock::now();
    auto fresh_position=[&] {
        return client.last_position && lab::seconds(lab::Clock::now(),*client.last_position)<0.6 &&
            std::isfinite(client.position.x) && std::isfinite(client.position.y) && std::isfinite(client.position.z);
    };
    auto fresh_landed=[&] {
        return client.last_landed && lab::seconds(lab::Clock::now(),*client.last_landed)<1.0;
    };
    while(lab::seconds(lab::Clock::now(),begin)<8 &&
          (!fresh_position() || !fresh_landed())) client.poll();
    if(!fresh_position() || !fresh_landed() || client.armed() ||
       client.landed_state!=MAV_LANDED_STATE_ON_GROUND)
        throw std::runtime_error("Need fresh local position, landed state and an initially disarmed vehicle");
    const float home_x=client.position.x,home_y=client.position.y,home_z=client.position.z;
    float x=home_x,y=home_y,z=home_z-2;
    auto next=lab::Clock::now();bool arm_attempted=false;
    auto stream=[&] {
        const auto now=lab::Clock::now();
        if(now>=next) {client.setpoint(x,y,z);next=now+std::chrono::milliseconds(50);}
    };
    auto command=[&](uint16_t id,std::array<float,7> p) {
        if(!client.command(id,p,stream)) throw std::runtime_error("Flight command failed");
    };
    auto land=[&] {
        // PX4 main mode AUTO=4, AUTO submode LAND=6. No geographic target ambiguity.
        std::array<float,7> p{};p[0]=MAV_MODE_FLAG_CUSTOM_MODE_ENABLED;p[1]=4;p[2]=6;
        command(MAV_CMD_DO_SET_MODE,p);
        if(!client.confirm([&]{return ((client.heartbeat.custom_mode>>16)&255)==4 &&
                                    (client.heartbeat.custom_mode>>24)==6;},5,stream))
            throw std::runtime_error("AUTO.LAND mode not observed");
        std::cout<<"LAND_MODE_CONFIRMED\n";
    };
    auto wait_landed=[&] {
        auto started=lab::Clock::now();
        while(lab::seconds(lab::Clock::now(),started)<40) {
            client.poll();
            if(!client.healthy(3)) throw std::runtime_error("Heartbeat stale while landing");
            if(fresh_landed() && client.landed_state==MAV_LANDED_STATE_ON_GROUND) {
                std::cout<<"LANDED_CONFIRMED\n";
                if(client.armed()) {
                    std::array<float,7> p{};
                    if(!client.command(MAV_CMD_COMPONENT_ARM_DISARM,p) ||
                       !client.confirm([&]{return !client.armed();},4))
                        throw std::runtime_error("Landed but disarm unconfirmed");
                }
                std::cout<<"DISARM_CONFIRMED\n";return;
            }
        }
        throw std::runtime_error("Landing confirmation timed out");
    };
    auto reach=[&](const char* phase) {
        auto started=lab::Clock::now();std::optional<lab::Time> stable;
        auto next_log=started;
        while(lab::seconds(lab::Clock::now(),started)<args.duration) {
            stream();client.poll();const auto now=lab::Clock::now();
            if(!client.healthy(3) || !fresh_position()) throw std::runtime_error("Flight telemetry stale");
            if(!client.offboard() || !client.armed()) throw std::runtime_error("Unexpected flight state");
            const auto& p=client.position;
            if(std::hypot(p.x-home_x,p.y-home_y)>6 || p.z<home_z-4 || p.z>home_z+0.7)
                throw std::runtime_error("Simulator exercise position bounds exceeded");
            float error=std::sqrt((p.x-x)*(p.x-x)+(p.y-y)*(p.y-y)+(p.z-z)*(p.z-z));
            if(now>=next_log) {
                std::cout<<"TRACK "<<phase<<" x="<<p.x<<" y="<<p.y<<" z="<<p.z<<" error="<<error<<'\n';
                next_log=now+std::chrono::seconds(1);
            }
            if(error<0.3f && std::hypot(std::hypot(p.vx,p.vy),p.vz)<0.4f) {
                if(!stable) stable=now;
                if(lab::seconds(now,*stable)>=1) {std::cout<<"REACHED "<<phase<<'\n';return;}
            } else stable.reset();
        }
        throw std::runtime_error(std::string("Position did not settle: ")+phase);
    };
    try {
        begin=lab::Clock::now();
        while(lab::seconds(lab::Clock::now(),begin)<2) {stream();client.poll();}
        std::array<float,7> p{};p[0]=MAV_MODE_FLAG_CUSTOM_MODE_ENABLED;p[1]=6;
        command(MAV_CMD_DO_SET_MODE,p);
        if(!client.confirm([&]{return client.offboard();},4,stream)) throw std::runtime_error("Offboard unconfirmed");
        std::cout<<"OFFBOARD_CONFIRMED\n";
        p={};p[0]=1;arm_attempted=true;command(MAV_CMD_COMPONENT_ARM_DISARM,p);
        if(!client.confirm([&]{return client.armed();},4,stream)) throw std::runtime_error("Arm unconfirmed");
        std::cout<<"ARM_CONFIRMED\n";
        reach("TAKEOFF");
        if(loss) {
            std::cout<<"STOP_SETPOINTS heartbeat_continues=1\n";
            if(!client.confirm([&]{return !client.offboard();},8)) throw std::runtime_error("No Offboard-loss transition");
            std::cout<<"OFFBOARD_LOSS_CONFIRMED custom_mode="<<client.heartbeat.custom_mode<<'\n';
            if(((client.heartbeat.custom_mode>>16)&255)!=4 || (client.heartbeat.custom_mode>>24)!=6)
                throw std::runtime_error("Expected configured AUTO.LAND failsafe");
        } else {
            x=home_x+2;reach("NORTH_2M");x=home_x;reach("RETURN_HOME");land();
        }
        wait_landed();std::cout<<(lite?"PX4LITE_PASS":"SITL_PASS")
            <<" scenario="<<(loss?"offboard-loss":"normal-flight")<<'\n';return 0;
    } catch(const std::exception& e) {
        std::cerr<<"FLIGHT_ERROR "<<e.what()<<'\n';
        if(arm_attempted) {
            try {land();wait_landed();} catch(const std::exception& recovery) {
                std::cerr<<"RECOVERY_UNCONFIRMED "<<recovery.what()<<'\n';
            }
        }
        return 3;
    }
});}
