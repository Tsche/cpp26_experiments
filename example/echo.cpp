#include <experiments/rpc.hpp>
#include <cstdio>

struct EchoService : rpc::Service {
    int echo(int arg, int arg2){
        printf("echo() called: %d %d\n", arg, arg2);
        return arg + arg2;
    }
    void foo() {
        puts("foo() called");
    }
};

template <typename T>
struct DummyClient : rpc::Remote<T> {
    T server;
    rpc::Message current{};

    explicit DummyClient(T server_) : server(server_){}


    void send(rpc::Message message){
        current = message;
    }

    auto recv(){
        auto ret = server.dispatch(current);
        return ret;
    }
};

int main(){
    auto service = EchoService{};
    auto client = DummyClient<EchoService>{service};

    auto msg = rpc::Message{.kind=rpc::Message::FNC_REQ, .opcode=1};
    service.dispatch(msg);

    printf("echo() return: %d\n", client.echo(client, 380, 40));
}