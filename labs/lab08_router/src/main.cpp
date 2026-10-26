#include "lab.hpp"
#include <thread>
// Fixed, unsigned, localhost teaching topology: one vehicle, two clients, no routing loops.
int main(int argc,char** argv) {return lab::run([&] {
    if(argc!=8) throw std::runtime_error(
        "usage: lab08_router flight_bind flight_peer gcs_bind gcs_peer client_bind client_peer seconds");
    std::array<uint16_t,6> ports{};
    for(size_t i=0;i<ports.size();++i) ports[i]=static_cast<uint16_t>(lab::number(argv[i+1],1,65535));
    int duration=lab::number(argv[7],1,3600);
    lab::Udp flight(ports[0],"127.0.0.1",ports[1]);
    lab::Udp gcs(ports[2],"127.0.0.1",ports[3]);
    lab::Udp client(ports[4],"127.0.0.1",ports[5]);
    auto start=lab::Clock::now();unsigned down=0,up=0;
    std::cout<<"ROUTER_READY\n";
    while(lab::seconds(lab::Clock::now(),start)<duration) {
        flight.receive(0,[&](const mavlink_message_t& m){gcs.send(m);client.send(m);++down;});
        gcs.receive(0,[&](const mavlink_message_t& m){flight.send(m);++up;});
        client.receive(0,[&](const mavlink_message_t& m){flight.send(m);++up;});
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    std::cout<<"ROUTER_SUMMARY telemetry="<<down<<" upstream="<<up<<'\n';return 0;
});}
