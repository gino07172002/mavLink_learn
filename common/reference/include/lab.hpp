#pragma once
// Single-threaded teaching transport. One parser per fixed UDP peer.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#endif
#include <common/mavlink.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace lab {
using Clock = std::chrono::steady_clock;
using Time = Clock::time_point;
inline double seconds(Time a, Time b) { return std::chrono::duration<double>(a-b).count(); }
inline uint32_t millis(Time start) { return static_cast<uint32_t>(seconds(Clock::now(),start)*1000); }
constexpr uint8_t client_sys = 245, client_comp = MAV_COMP_ID_ONBOARD_COMPUTER;
constexpr uint16_t position_mask = POSITION_TARGET_TYPEMASK_VX_IGNORE |
    POSITION_TARGET_TYPEMASK_VY_IGNORE | POSITION_TARGET_TYPEMASK_VZ_IGNORE |
    POSITION_TARGET_TYPEMASK_AX_IGNORE | POSITION_TARGET_TYPEMASK_AY_IGNORE |
    POSITION_TARGET_TYPEMASK_AZ_IGNORE | POSITION_TARGET_TYPEMASK_YAW_RATE_IGNORE;
constexpr uint32_t px4_offboard = 6u << 16;

class Parser {
    mavlink_message_t buffer_{};
    mavlink_status_t state_{};
public:
    unsigned bad_frames = 0;
    template<class F> void feed(const uint8_t* bytes, size_t n, F on_message) {
        for(size_t i=0; i<n; ++i) {
            mavlink_message_t msg{};
            mavlink_status_t status{};
            auto result = mavlink_frame_char_buffer(&buffer_, &state_, bytes[i], &msg, &status);
            if(result == MAVLINK_FRAMING_OK) on_message(msg);
            else if(result == MAVLINK_FRAMING_BAD_CRC || result == MAVLINK_FRAMING_BAD_SIGNATURE)
                ++bad_frames;
        }
    }
};
inline std::vector<uint8_t> encode(const mavlink_message_t& msg) {
    std::array<uint8_t, MAVLINK_MAX_PACKET_LEN> bytes{};
    auto n=mavlink_msg_to_send_buffer(bytes.data(), &msg);
    return {bytes.begin(), bytes.begin()+n};
}
class NetworkRuntime {
public:
    NetworkRuntime() {
#ifdef _WIN32
        WSADATA data{};
        if(WSAStartup(MAKEWORD(2,2), &data)!=0) throw std::runtime_error("WSAStartup failed");
#endif
    }
    ~NetworkRuntime() {
#ifdef _WIN32
        WSACleanup();
#endif
    }
};
#ifdef _WIN32
using Socket = SOCKET;
constexpr Socket invalid_socket = INVALID_SOCKET;
inline void close_socket(Socket fd) { closesocket(fd); }
inline int socket_error() { return WSAGetLastError(); }
#else
using Socket = int;
constexpr Socket invalid_socket = -1;
inline void close_socket(Socket fd) { close(fd); }
inline int socket_error() { return errno; }
#endif
class Udp {
    Socket fd_ = invalid_socket;
    sockaddr_in peer_{};
    Parser parser_;
    [[noreturn]] static void fail(const char* action) {
        throw std::runtime_error(std::string(action)+" socket error="+std::to_string(socket_error()));
    }
public:
    Udp(uint16_t local_port, const std::string& peer_ip, uint16_t peer_port) {
        peer_.sin_family=AF_INET; peer_.sin_port=htons(peer_port);
        if(inet_pton(AF_INET,peer_ip.c_str(),&peer_.sin_addr)!=1)
            throw std::runtime_error("peer must be an IPv4 address");
        fd_=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
        if(fd_==invalid_socket) fail("socket");
        sockaddr_in local{}; local.sin_family=AF_INET;
        local.sin_addr.s_addr=htonl(INADDR_ANY); local.sin_port=htons(local_port);
        if(bind(fd_,reinterpret_cast<sockaddr*>(&local),sizeof(local))!=0) {
            auto error=socket_error(); close_socket(fd_); fd_=invalid_socket;
            throw std::runtime_error("bind socket error="+std::to_string(error));
        }
    }
    ~Udp() { if(fd_!=invalid_socket) close_socket(fd_); }
    Udp(const Udp&)=delete;
    Udp& operator=(const Udp&)=delete;
    void send(const mavlink_message_t& msg) { send_bytes(encode(msg)); }
    void send_bytes(const std::vector<uint8_t>& bytes) {
        int n=sendto(fd_,reinterpret_cast<const char*>(bytes.data()),static_cast<int>(bytes.size()),0,
            reinterpret_cast<const sockaddr*>(&peer_),sizeof(peer_));
        if(n!=static_cast<int>(bytes.size())) fail("sendto");
    }
    template<class F> void receive(int timeout_ms,F on_message) {
        fd_set readfds; FD_ZERO(&readfds); FD_SET(fd_,&readfds);
        timeval wait{timeout_ms/1000,(timeout_ms%1000)*1000};
        int ready=select(static_cast<int>(fd_+1),&readfds,nullptr,nullptr,&wait);
        if(ready<0) fail("select");
        if(ready==0) return;
        // Full UDP payload buffer: never treat truncated datagrams as valid input.
        std::array<uint8_t,65536> bytes{}; sockaddr_in from{};
#ifdef _WIN32
        int len=sizeof(from);
#else
        socklen_t len=sizeof(from);
#endif
        int n=recvfrom(fd_,reinterpret_cast<char*>(bytes.data()),static_cast<int>(bytes.size()),0,
            reinterpret_cast<sockaddr*>(&from),&len);
        if(n<0) {
#ifdef _WIN32
            // Unconnected UDP may surface ICMP port-unreachable from an earlier send.
            // Let discovery/heartbeat deadlines report peer absence, as on POSIX.
            if(WSAGetLastError()==WSAECONNRESET) return;
#endif
            fail("recvfrom");
        }
        if(from.sin_addr.s_addr!=peer_.sin_addr.s_addr || from.sin_port!=peer_.sin_port) return;
        parser_.feed(bytes.data(),static_cast<size_t>(n),on_message);
    }
};
inline int number(const char* text, int minimum,int maximum) {
    size_t used=0; int value=std::stoi(text,&used);
    if(used!=std::string(text).size() || value<minimum || value>maximum)
        throw std::runtime_error("argument out of range");
    return value;
}
struct Args {
    uint16_t local=14550, remote=14560;
    std::string ip="127.0.0.1";
    int duration=6;
    void parse(int argc,char** argv,int offset) {
        if(argc>offset) local=static_cast<uint16_t>(number(argv[offset],1,65535));
        if(argc>offset+1) ip=argv[offset+1];
        if(argc>offset+2) remote=static_cast<uint16_t>(number(argv[offset+2],1,65535));
        if(argc>offset+3) duration=number(argv[offset+3],1,3600);
        if(argc>offset+4) throw std::runtime_error("too many arguments");
    }
};
inline const char* result_name(uint8_t result) {
    switch(result) {
    case MAV_RESULT_ACCEPTED:return "ACCEPTED";
    case MAV_RESULT_TEMPORARILY_REJECTED:return "TEMPORARILY_REJECTED";
    case MAV_RESULT_DENIED:return "DENIED";
    case MAV_RESULT_UNSUPPORTED:return "UNSUPPORTED";
    case MAV_RESULT_FAILED:return "FAILED";
    case MAV_RESULT_IN_PROGRESS:return "IN_PROGRESS";
    case MAV_RESULT_CANCELLED:return "CANCELLED";
    default:return "OTHER";
    }
}
inline bool ack_matches(const mavlink_message_t& msg,const mavlink_command_ack_t& ack,
                        uint8_t sys,uint8_t comp,uint16_t command) {
    return msg.sysid==sys && msg.compid==comp && ack.command==command &&
        (ack.target_system==0 || ack.target_system==client_sys) &&
        (ack.target_component==0 || ack.target_component==client_comp);
}
class Client {
    Udp udp_;
    Time start_=Clock::now(), last_send_=Time::min();
    std::optional<mavlink_command_ack_t> ack_;
    std::optional<uint16_t> pending_;
public:
    uint8_t sys=0,comp=0;
    mavlink_heartbeat_t heartbeat{};
    std::optional<Time> last_heartbeat;
    uint64_t heartbeat_count=0;
    mavlink_local_position_ned_t position{};
    std::optional<Time> last_position;
    uint8_t landed_state=MAV_LANDED_STATE_UNDEFINED;
    std::optional<Time> last_landed;
    std::function<void(const mavlink_message_t&)> observer;
    explicit Client(const Args& args):udp_(args.local,args.ip,args.remote) {}
    void send(const mavlink_message_t& message) {udp_.send(message);}
    void poll(int wait=20) {
        const auto now=Clock::now();
        if(last_send_==Time::min() || seconds(now,last_send_)>=1) {
            mavlink_message_t msg{};
            mavlink_msg_heartbeat_pack(client_sys,client_comp,&msg,MAV_TYPE_GCS,
                MAV_AUTOPILOT_INVALID,0,0,MAV_STATE_ACTIVE);
            udp_.send(msg); last_send_=now;
        }
        udp_.receive(wait,[&](const mavlink_message_t& msg) {
            if(msg.msgid==MAVLINK_MSG_ID_HEARTBEAT) {
                mavlink_heartbeat_t hb{}; mavlink_msg_heartbeat_decode(&msg,&hb);
                if(!sys && msg.sysid!=client_sys && hb.autopilot!=MAV_AUTOPILOT_INVALID &&
                   hb.type!=MAV_TYPE_GCS) {sys=msg.sysid;comp=msg.compid;}
                if(msg.sysid==sys && msg.compid==comp) {
                    heartbeat=hb; last_heartbeat=Clock::now(); ++heartbeat_count;
                }
            }
            if(msg.sysid!=sys || msg.compid!=comp) return;
            if(msg.msgid==MAVLINK_MSG_ID_LOCAL_POSITION_NED) {
                mavlink_msg_local_position_ned_decode(&msg,&position);last_position=Clock::now();
            }
            if(msg.msgid==MAVLINK_MSG_ID_EXTENDED_SYS_STATE) {
                mavlink_extended_sys_state_t state{};
                mavlink_msg_extended_sys_state_decode(&msg,&state);
                landed_state=state.landed_state;last_landed=Clock::now();
            }
            if(msg.msgid==MAVLINK_MSG_ID_STATUSTEXT) {
                mavlink_statustext_t status{};mavlink_msg_statustext_decode(&msg,&status);
                std::cout<<"STATUSTEXT severity="<<int(status.severity)<<" "
                    <<std::string(status.text,std::find(status.text,status.text+sizeof(status.text),'\0'))<<'\n';
            }
            if(msg.msgid==MAVLINK_MSG_ID_COMMAND_ACK && pending_) {
                mavlink_command_ack_t ack{};mavlink_msg_command_ack_decode(&msg,&ack);
                if(ack_matches(msg,ack,sys,comp,*pending_)) ack_=ack;
            }
            if(observer) observer(msg);
        });
    }
    bool discover(double timeout=3) {
        auto begin=Clock::now();
        while(seconds(Clock::now(),begin)<timeout) { poll(); if(sys) return true; }
        std::cerr<<"DISCOVERY_TIMEOUT\n";return false;
    }
    bool armed() const {return (heartbeat.base_mode&MAV_MODE_FLAG_SAFETY_ARMED)!=0;}
    bool offboard() const {return (heartbeat.custom_mode&0x00ff0000u)==px4_offboard;}
    bool healthy(double timeout=2) const {
        return last_heartbeat && seconds(Clock::now(),*last_heartbeat)<timeout;
    }
    // Only one outstanding command. service keeps the Offboard stream running while waiting.
    bool command(uint16_t id,std::array<float,7> params,
                 const std::function<void()>& service=[] {}) {
        pending_=id; ack_.reset();
        for(uint8_t attempt=0;attempt<3;++attempt) {
            mavlink_message_t msg{};
            mavlink_msg_command_long_pack(client_sys,client_comp,&msg,sys,comp,id,attempt,
                params[0],params[1],params[2],params[3],params[4],params[5],params[6]);
            udp_.send(msg); std::cout<<"COMMAND id="<<id<<" attempt="<<int(attempt+1)<<'\n';
            auto begin=Clock::now();bool in_progress=false;
            while(seconds(Clock::now(),begin)<(in_progress?5.0:0.6)) {
                service();poll();
                if(!ack_) continue;
                auto ack=*ack_;ack_.reset();
                std::cout<<"ACK "<<result_name(ack.result)<<" result="<<int(ack.result)
                    <<" progress="<<int(ack.progress)<<" detail="<<ack.result_param2<<'\n';
                if(ack.result==MAV_RESULT_IN_PROGRESS) {in_progress=true;continue;}
                pending_.reset();return ack.result==MAV_RESULT_ACCEPTED;
            }
            if(in_progress) {std::cerr<<"COMMAND_EXECUTION_TIMEOUT\n";pending_.reset();return false;}
        }
        pending_.reset();std::cerr<<"COMMAND_TIMEOUT (outcome unknown)\n";return false;
    }
    bool confirm(const std::function<bool()>& predicate,double timeout=2.5,
                 const std::function<void()>& service=[] {}) {
        const auto previous=heartbeat_count;const auto begin=Clock::now();
        while(seconds(Clock::now(),begin)<timeout) {
            service();poll();
            if(heartbeat_count>previous && healthy() && predicate()) return true;
        }
        std::cerr<<"STATE_CONFIRM_TIMEOUT\n";return false;
    }
    void setpoint(float x,float y,float z) {
        mavlink_message_t msg{};
        mavlink_msg_set_position_target_local_ned_pack(client_sys,client_comp,&msg,millis(start_),
            sys,comp,MAV_FRAME_LOCAL_NED,position_mask,x,y,z,0,0,0,0,0,0,0,0);
        udp_.send(msg);
    }
};
template<class F> int run(F body) {
    try {NetworkRuntime network;std::cout.setf(std::ios::unitbuf);return body();}
    catch(const std::exception& e) {std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;}
}
} // namespace lab
