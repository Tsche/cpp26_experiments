#include <cstdio>
#include <erl/rpc/rpc.hpp>
#include <erl/message.hpp>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

template <typename P>
struct TCPTransport : P {
  int fd = -1;
  ~TCPTransport() { close(fd); }
  P::message_type recv() {
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
    auto msg  = typename P::message_type{.kind     = typename P::message_type::Flags(header[0]),
                             .opcode   = header[1],
                             .checksum = std::uint16_t(header[3] << 8 | header[2]),
                             .size     = 0};
    msg.reserve(size);
    msg.header.size = size;
    cursor          = 0;
    while (cursor != size) {
      auto ret = read(fd, (char*)msg.data() + cursor, size - cursor);
      if (ret < 0) {
        // throw std::runtime_error("");
      }
      cursor += ret;
    }
    return msg;
  }

  void send(P::message_type message) {
    write(fd, (char*)&message.kind, sizeof(message.kind));
    write(fd, (char*)&message.opcode, sizeof(message.opcode));
    write(fd, (char*)&message.checksum, sizeof(message.checksum));
    write(fd, (char*)&message.size, sizeof(message.size));
    write(fd, message.data(), message.size);
  }

  void connect(char const* host, int port) {
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
      // throw std::runtime_error("Could not open socket.");
    }

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = inet_addr(host);
    addr.sin_port        = port;
    if (::connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
      // throw std::runtime_error{"Could not connect."};
    }
  }
};

template <typename P>
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
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = port;
    if (bind(sockfd, (sockaddr*)&addr, sizeof(addr)) < 0) {
      // throw std::runtime_error{"Could not bind."};
    }

    ::listen(sockfd, 5);
  }

  void loop(auto& service) {
    sockaddr_in addr{};
    socklen_t addr_len;
    int client = accept(sockfd, (sockaddr*)&addr, &addr_len);
    if (client < 0) {
      // throw std::runtime_error("Error in accept");
    }

    auto transport = TCPTransport<P>{client};
    auto message   = transport.recv();
    auto ret       = service.template dispatch<P>(message);
    transport.send(ret);
    ::close(client);
  }

private:
  int sockfd;
};

template <typename T>
concept is_transport = requires(T obj) {
  { obj.recv() } -> std::same_as<typename T::message_type>;
  { obj.send(std::declval<typename T::message_type>()) } -> std::same_as<void>;
};

struct Protocol {
  using message_type = rpc::Message;

  template <typename Func, typename Obj>
  static auto make_response(Obj&& obj, auto& args){
    auto ret = rpc::Message{{.kind = rpc::Message::Flags::RET, .opcode = Func::index}};
    if constexpr (std::is_void_v<typename Func::return_type>) {
      eval(std::forward<Obj>(obj), args);
    } else {
      auto retval = Func::eval(std::forward<Obj>(obj), args);
      // ret.add_field(retval);
    }
    // return ret.finalize();
    return ret;
  }

  template <typename Func, typename... Args>
  static auto make_request(Args... args){
    auto msg = rpc::Message{.kind = rpc::Message::Flags::REQ, .opcode = Func::index};
    // (msg.add_field(args), ...);
    return msg;
  }
};


template <typename P>
struct TCPClient : TCPTransport<P> {
  template <typename R>
  R call(P::message_type message) {
    TCPTransport<P>::send(message);
    auto response = TCPTransport<P>::recv();
    if constexpr (!std::is_void_v<R>) {
      auto parser = rpc::MessageParser{response.payload()};
      return parser.template get_field<std::remove_cvref_t<R>>();
    }
  }
};
#include <unordered_map>

struct Test {
  char foo[4];
  short bar;

  struct Inner {
    bool a;
    enum { FOO = 4, BAR = 5 } b;
  };
  std::unordered_map<int, Inner> zoinks;
};

struct TestService : rpc::Service {
  int add(int arg, int arg2) {
    printf("add() called: %d %d\n", arg, arg2);
    return arg + arg2;
  }

  void test(std::vector<int> arr) {
    for (auto&& e : arr) {
      printf("%d, ", e);
    }
    puts("");
  }
};

int main() {
  int pid                    = fork();
  static constexpr auto port = 5324;
  if (pid > 0) {
    auto service = TestService{};
    auto server  = TCPServer<Protocol>{};
    server.listen(port);
    server.loop(service);
  } else {
    auto client = TCPClient<Protocol>{};
    auto remote = rpc::make_proxy<TestService>(client);
    client.connect("127.0.0.1", port);
    // auto x = remote.add(380, 40);
    // printf("add() return: %d\n", x);
    // auto map = std::unordered_map<int, Test::Inner>{{5, {true, Test::Inner::FOO}}, {42, {false, Test::Inner::BAR}}};
    remote.test(std::vector<int>{1, 5, 7, 31});
  }
}