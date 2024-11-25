#include <cstdio>
#include <experiments/rpc.hpp>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
struct TCPTransport {
  int fd = -1;
  ~TCPTransport() { close(fd); }
  rpc::Message recv() {
    unsigned char header[8]{};
    int cursor = 0;
    while (cursor != 8) {
      auto ret = read(fd, header + cursor, 8 - cursor);
      if (ret < 0) {
        // throw std::runtime_error("");
      }
      cursor += ret;
    }
    auto size = std::uint32_t(header[7] << 24 | header[6] << 16 | header[5] << 8 | header[4]);
    auto msg =
        rpc::Message{{.kind = rpc::Message::Flags(header[0]),
                     .opcode = header[1],
                     .checksum = std::uint16_t(header[3] << 8 | header[2]),
                     .size = 0}};
    msg.reserve(size);
    cursor = 0;
    while (cursor != size) {
      auto ret = read(fd, (char*)msg.data() + cursor, size - cursor);
      if (ret < 0) {
        // throw std::runtime_error("");
      }
      cursor += ret;
    }
    return msg;
  }

  void send(rpc::Message message) {
    write(fd, (char *)&message.header.kind, sizeof(message.header.kind));
    write(fd, (char *)&message.header.opcode, sizeof(message.header.opcode));
    write(fd, (char *)&message.header.checksum, sizeof(message.header.checksum));
    write(fd, (char *)&message.header.size, sizeof(message.header.size));
    write(fd, message.data(), message.header.size);
  }

  void connect(char const *host, int port) {
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
      // throw std::runtime_error("Could not open socket.");
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(host);
    addr.sin_port = port;
    if (::connect(fd, (sockaddr *)&addr, sizeof(addr)) < 0) {
      // throw std::runtime_error{"Could not connect."};
    }
  }
};

struct TCPClient : TCPTransport {
  template <typename R> R call(rpc::Message message) {
    TCPTransport::send(message);
    auto response = TCPTransport::recv();
    if constexpr (!std::is_void_v<R>) {
      auto parser = rpc::MessageParser{response.payload()};
      return parser.template get_field<std::remove_cvref_t<R>>();
    }
  }
};

template <typename... Services>
struct Server {
    
};

struct TCPServer {
  TCPServer() = default;
//   explicit TCPServer(T *service_) : service(service_) {}
  ~TCPServer() { close(sockfd); }

  void listen(int port) {
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
      // throw std::runtime_error("Could not open socket.");
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = port;
    if (bind(sockfd, (sockaddr *)&addr, sizeof(addr)) < 0) {
      // throw std::runtime_error{"Could not bind."};
    }

    ::listen(sockfd, 5);
  }

  void loop(auto& service) {
    sockaddr_in addr{};
    socklen_t addr_len;
    int client = accept(sockfd, (sockaddr *)&addr, &addr_len);
    if (client < 0) {
      // throw std::runtime_error("Error in accept");
    }

    auto transport = TCPTransport{client};
    auto message = transport.recv();
    auto ret = service.dispatch(message);
    transport.send(ret);
    ::close(client);
  }

private:
  int sockfd;
};

#include <unordered_map>

struct Test{
  char foo[4];
  short bar;

  struct Inner{
    bool a;
    enum {
      FOO = 4,
      BAR = 5
    } b;
  };
  std::unordered_map<int, Inner> zoinks;
};

struct EchoService : rpc::Service {
  int add(int arg, int arg2) {
    printf("add() called: %d %d\n", arg, arg2);
    return arg + arg2;
  }

  void test(std::vector<int> arr){
    for (auto&& e : arr){
      printf("%d, ", e);
    }
    puts("");
  }
};


int main() {
  int pid = fork();
  static constexpr auto port = 5321;
  if (pid > 0) {
    auto service = EchoService{};
    auto server = TCPServer{};
    server.listen(port);
    server.loop(service);
  } else {
    auto client = TCPClient{};
    auto remote = rpc::make_proxy<EchoService>(client);
    client.connect("127.0.0.1", port);
    // auto x = remote.add(380, 40);
    // printf("add() return: %d\n", x);
    // auto map = std::unordered_map<int, Test::Inner>{{5, {true, Test::Inner::FOO}}, {42, {false, Test::Inner::BAR}}};
    remote.test(std::vector<int>{1,5,7,31});
  }
}