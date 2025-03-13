#include <unistd.h>
#include <cstddef>
#include <cassert>
#include <cstdint>
#include <print>
#include <iostream>

#include <erl/rpc.hpp>

#include <thread>
#include "erl/log/message.hpp"
#include "erl/net/message/reader.hpp"



using namespace std::string_literals;

struct TestService {
  using policy = erl::rpc::InProcess;

  void print_num(int num) { std::println("print_num: {}", num); }
  void print(auto... vals) {
    std::print("print: ");
    (std::print("{} ", vals), ...);
    std::println("");
  }
  int accumulate(int first, auto... num) { return (first + ... + num); }
};



#include <erl/log/logger.hpp>
namespace erl::logging {

struct LoggingService {
  // std::vector<std::unique_ptr<Sink>> sinks;

  using policy       = rpc::Annotated;
  using message_type = erl::message::HybridBuffer<58>;
  using protocol     = rpc::RPCProtocol<message_type>;

  [[=rpc::callback]] void spawn(std::uint64_t thread){std::print("spawn {}", thread);}
  [[=rpc::callback]] void exit(std::uint64_t thread){}
  [[=rpc::callback]] void rename(std::uint64_t thread, std::string_view name){}
  [[=rpc::callback]] void set_parent(std::uint64_t thread, std::uint64_t parent){}
  [[=rpc::callback]] void add_sink(Sink* sink){}
  [[=rpc::callback]] void remove_sink(Sink* sink){}

private:
  void handle_print(std::span<char const> data){
    auto reader = erl::message::MessageView{data};
    auto severity = deserialize<Severity>(reader);
    auto fnc = deserialize<formatter_type>(reader);
    // auto prelude = deserialize<LoggingEvent>(reader);
    // auto fnc     = reinterpret_cast<formatter_type>(prelude.handler_ptr);
    auto [location, text] = fnc(reader.buffer.subspan(reader.cursor));
    // auto timestamp = timestamp_t{std::chrono::system_clock::duration{prelude.timestamp}};
    // auto message = Message{
    //   .severity = prelude.severity,
    //   .thread = {},
    //   .timestamp = timestamp,
    //   .location = location,
    //   .text = text
    // };

    std::print("foo;  {}", text);
  }

public:
  template <typename... Args>
  [[= rpc::handler(^^LoggingService::handle_print)]]
  static auto print(erl::logging::Severity severity, erl::logging::formatter_type formatter, Args&&... args) {
    auto message   = Logger::message_type{};
    auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    auto event     = LoggingEvent{.severity    = severity,
                                  .thread      = erl::this_thread,
                                  .timestamp   = timestamp,
                                  .handler     = formatter};
    // serialize(event, message);
    serialize(formatter, message);
    (serialize(std::forward<Args>(args), message), ...);
    return message;
  }
};
}  // namespace erl::logging


auto logging_pipe = erl::EventQueue<erl::logging::LoggingService::message_type>();

template <typename... Args>
void logg(erl::logging::FormatString<Args...> fmt, Args&&... args) {
  auto client = logging_pipe.make_client();
  auto remote = erl::rpc::make_proxy<erl::logging::LoggingService>(&client);
  remote.print(erl::logging::INFO, fmt.format, std::forward<Args>(args)...);
}


int main() {
  using namespace erl::logging;

  auto x = std::jthread([&] {
    // auto service = TestService{};
    auto server  = logging_pipe.make_server();

    auto service = erl::logging::LoggingService{};
    server.run(service);
  });

  auto client = logging_pipe.make_client();
  auto remote = erl::rpc::make_proxy<LoggingService>(&client);
  // auto remote = erl::rpc::make_proxy<TestService>(&client);

  // logg("foo {}\n", 42);
  remote.spawn(42);

  // erl::rpc::call<&LoggingService::spawn>(client, 42);

  client.kill();
}