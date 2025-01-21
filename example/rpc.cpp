// #include <slo/net/tcp.hpp>
// #include <slo/net/rpc.hpp>

// template <typename Server>
// struct ConnectionManager : Server {
//   void accept_loop(){

//   }
// private:
//   std::vector<slo::tcp::Client> clients;
// };


// struct TestService : slo::rpc::Service {
//   int echo(int num) { return num; }
// };

// int main(){
//   int pid = fork();
//   static constexpr int port = 4206;
//   if (pid > 0){
//     auto service = TestService{};
//     auto server = slo::tcp::Server();
//     server.listen(port, 1);
//     auto client = server.accept();

//   } else {
//     auto client = slo::tcp::Client();
//     client.connect("127.0.0.1", port);

//     auto remote = slo::rpc::make_proxy<TestService>(client);
//   }
// }

#include <iostream>
#include <thread>

void pinThread(int cpu) {
  if (cpu < 0) {
    return;
  }
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(cpu, &cpuset);
  if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) ==
      -1) {
    perror("pthread_setaffinity_no");
    exit(1);
  }
}

template <typename T> void bench(int cpu1, int cpu2) {
  // const size_t queueSize = 100000;
  const int64_t iters = 10'000'000;

  T q{};
  auto t = std::thread([&] {
    pinThread(cpu1);
    for (int i = 0; i < iters; ++i) {
      int val;
      while (!q.try_pop(&val));
      if (val != i) {
        throw std::runtime_error("");
      }
    }
  });

  pinThread(cpu2);

  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < iters; ++i) {
    q.push(i);
  }
  while (!q.is_empty())
    ;
  auto stop = std::chrono::steady_clock::now();
  t.join();
  std::cout << iters * 1000000000 /
                   std::chrono::duration_cast<std::chrono::nanoseconds>(stop -
                                                                        start)
                       .count()
            << " msg/s" << std::endl;
}

#include <slo/queue/mpmc_bounded.hpp>
#include <slo/queue/spsc_bounded.hpp>
#include <slo/queue/basic.hpp>

int main(int argc, char *argv[]) {
  int strategy = -1;
  int cpu1 = -1;
  int cpu2 = -1;
  if (argc < 2){
    return 0;
  }
  strategy = std::stoi(argv[1]);
  if (argc == 4) {
    cpu1 = std::stoi(argv[2]);
    cpu2 = std::stoi(argv[3]);
  }

  switch(strategy){
    case 0:
    bench<slo::BasicQueue<int>>(cpu1, cpu2);
    break;
    case 1: 
    bench<slo::queues::BoundedSPSC<int, 64>>(cpu1, cpu2);
    break;
    case 2: 
    bench<slo::queues::BoundedMPMC<int, 64>>(cpu1, cpu2);
    break;
  }
  

  return 0;
}