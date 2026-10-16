/**
 * @file  main.cpp
 * @brief Program entry: configuration, sockets, event loop.
 *        프로그램 시작점: 설정, 소켓, 이벤트 루프.
 */
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "config/ConfigError.hpp"
#include "config/ConfigLoader.hpp"
#include "core/Clock.hpp"
#include "core/EventLoop.hpp"
#include "core/Limits.hpp"
#include "core/Server.hpp"
#include "core/Signals.hpp"

namespace {

/**
 * @brief Prints "webserv: message" to stderr.
 *        stderr에 "webserv: message"를 출력한다.
 * @param[in] message What went wrong. / 무엇이 잘못됐는지.
 * @return EXIT_FAILURE, for "return fail(...)" in main().
 *         main()에서 "return fail(...)"로 쓰도록 EXIT_FAILURE.
 */
int fail(const std::string& message) {
  std::cerr << "webserv: " << message << std::endl;
  return EXIT_FAILURE;
}

}  // namespace

/**
 * @brief Loads the configuration, opens the listen sockets and runs the
 *        event loop until SIGINT or SIGTERM.
 *        설정을 읽고, listen 소켓을 열고, SIGINT나 SIGTERM이 올 때까지
 *        이벤트 루프를 돌린다.
 * @param[in] argc Argument count, 1 or 2. / 인자 개수, 1 또는 2.
 * @param[in] argv Program name and an optional configuration file.
 *                 프로그램 이름과 선택적 설정 파일.
 * @return EXIT_SUCCESS after a clean shutdown, EXIT_FAILURE otherwise.
 *         깨끗하게 끝나면 EXIT_SUCCESS, 아니면 EXIT_FAILURE.
 */
int main(int argc, char** argv) {
  if (argc > limits::kMaxArgc) {
    std::cerr << "usage: " << argv[0] << " [configuration file]" << std::endl;
    return EXIT_FAILURE;
  }
  const char* configPath =
      (argc == limits::kMaxArgc) ? argv[1] : limits::kDefaultConfigPath;
  std::vector<ServerConfig> configs;
  try {
    configs = loadConfigFile(configPath);
  } catch (const ConfigError& error) {
    return fail(error.what());
  }

  Clock clock(limits::kClockFile);
  if (!clock.refresh())
    return fail(std::string("cannot use clock file ") + limits::kClockFile);
  signals::install();

  EventLoop loop(clock);
  Server server(configs, loop, clock.now());
  std::string error;
  if (!server.openListeners(error)) return fail(error);
  std::cout << "webserv: listening (" << configPath << ")" << std::endl;
  // Exceptions from a single connection are handled inside the loop. One
  // that still gets here (std::bad_alloc while polling) ends the server
  // with a message instead of std::terminate().
  // 연결 하나에서 난 예외는 루프 안에서 처리한다. 그래도 여기까지 오는
  // 예외(poll 중의 std::bad_alloc)는 std::terminate() 대신 메시지를 내고
  // 서버를 끝낸다.
  try {
    loop.run(server);
  } catch (const std::exception& error) {
    return fail(std::string("stopped: ") + error.what());
  }
  std::cout << "webserv: shutting down" << std::endl;
  return EXIT_SUCCESS;
}
