#include "lab.hpp"
int main() {return lab::run([] {
    // Public deterministic TEST key; do not use this in a deployed link.
    mavlink_signing_t tx{};tx.flags=MAVLINK_SIGNING_FLAG_SIGN_OUTGOING;tx.link_id=7;tx.timestamp=100000;
    for(size_t i=0;i<sizeof(tx.secret_key);++i) tx.secret_key[i]=static_cast<uint8_t>(i+1);
    mavlink_status_t outgoing{};outgoing.signing=&tx;mavlink_message_t message{};
    mavlink_msg_heartbeat_pack_status(1,1,&outgoing,&message,MAV_TYPE_QUADROTOR,MAV_AUTOPILOT_GENERIC,0,0,MAV_STATE_ACTIVE);
    const auto signed_bytes=lab::encode(message);
    auto require=[](bool value,const char* label) {
        if(!value) throw std::runtime_error(label);
        std::cout<<"PASS "<<label<<'\n';
    };
    require((message.incompat_flags&MAVLINK_IFLAG_SIGNED)!=0,"outgoing signed flag");
    mavlink_signing_t verifier{};std::copy(std::begin(tx.secret_key),std::end(tx.secret_key),verifier.secret_key);
    verifier.timestamp=100000;
    mavlink_signing_streams_t streams{};mavlink_status_t receiving{};
    receiving.signing=&verifier;receiving.signing_streams=&streams;
    mavlink_message_t buffer{},parsed{};
    auto feed=[&](const std::vector<uint8_t>& bytes) {
        uint8_t result=MAVLINK_FRAMING_INCOMPLETE;
        for(auto b:bytes) {mavlink_status_t status{};result=mavlink_frame_char_buffer(&buffer,&receiving,b,&parsed,&status);}
        return result;
    };
    require(feed(signed_bytes)==MAVLINK_FRAMING_OK,"valid signature accepted");
    require(feed(signed_bytes)==MAVLINK_FRAMING_BAD_SIGNATURE,"replayed packet rejected");
    streams={};auto changed=signed_bytes;changed.back()^=1;
    require(feed(changed)==MAVLINK_FRAMING_BAD_SIGNATURE,"tampered signature rejected");
    streams={};verifier.secret_key[0]^=1;
    require(feed(signed_bytes)==MAVLINK_FRAMING_BAD_SIGNATURE,"wrong secret key rejected");
    verifier.secret_key[0]^=1;streams={};mavlink_message_t plain{};
    mavlink_msg_heartbeat_pack(1,1,&plain,MAV_TYPE_QUADROTOR,MAV_AUTOPILOT_GENERIC,0,0,MAV_STATE_ACTIVE);
    require(feed(lab::encode(plain))==MAVLINK_FRAMING_BAD_SIGNATURE,"unsigned packet rejected by strict policy");
    std::cout<<"SIGNING_CHECKS=6\n";return 0;
});}
