#include "lab.hpp"
int main(int argc,char** argv) {return lab::run([&] {
    if(argc<2 || (std::string(argv[1])!="arm" && std::string(argv[1])!="disarm"))
        throw std::runtime_error("usage: lab02_command arm|disarm [local_port peer_ip peer_port duration_unused]");
    bool arm=std::string(argv[1])=="arm";
    lab::Args args;args.parse(argc,argv,2);lab::Client client(args);
    if(!client.discover()) return 2;
    std::array<float,7> params{};params[0]=arm?1.0f:0.0f;
    if(!client.command(MAV_CMD_COMPONENT_ARM_DISARM,params)) return 3;
    if(!client.confirm([&]{return client.armed()==arm;})) return 4;
    std::cout<<"STATE_CONFIRMED armed="<<client.armed()<<'\n';return 0;
});}
