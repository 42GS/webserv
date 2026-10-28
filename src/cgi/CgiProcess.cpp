/**
 * @file  CgiProcess.cpp
 * @brief Starting a CGI script and following it.
 *        CGI 스크립트 시작과 추적.
 *
 * The steps follow wiki concepts/cgi section 1 and design decision 12.
 * 단계는 위키 concepts/cgi 1절과 설계 결정 12를 따른다.
 */
#include "cgi/CgiProcess.hpp"

#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <climits>
#include <cstdlib>
#include <exception>
#include <iostream>

#include "core/Limits.hpp"
#include "http/Ascii.hpp"

namespace {

/**
 * @brief Lists the descriptors open in this process.
 *        이 프로세스에 열린 디스크립터 목록.
 */
const char* const kDescriptorDirectory = "/dev/fd";
/**
 * @brief First descriptor after standard input, output and error.
 *        표준 입력·출력·에러 다음의 첫 디스크립터.
 */
const int kFirstOtherDescriptor = STDERR_FILENO + 1;

/**
 * @brief Closes both ends of a pipe.
 *        pipe의 양 끝을 닫는다.
 * @param[in] ends The two descriptors. / 디스크립터 두 개.
 */
void closePipe(const int ends[2]) {
  close(ends[0]);
  close(ends[1]);
}

/**
 * @brief Creates the input and the output pipe.
 *        입력 pipe와 출력 pipe를 만든다.
 * @param[out] input  Ends of the input pipe. / 입력 pipe의 두 끝.
 * @param[out] output Ends of the output pipe. / 출력 pipe의 두 끝.
 * @return False if either could not be created; nothing is left open.
 *         어느 하나라도 만들지 못하면 false. 열린 것은 남기지 않는다.
 */
bool openPipes(int input[2], int output[2]) {
  if (pipe(input) != 0) return false;
  if (pipe(output) == 0) return true;
  closePipe(input);
  return false;
}

/**
 * @brief Pointers to the texts, ending in a null pointer, for execve().
 *        execve()에 넘길, 널 포인터로 끝나는 텍스트 포인터 목록.
 * @param[in] texts Texts that outlive the result. / 결과보다 오래 사는 텍스트.
 * @return The pointers. / 포인터 목록.
 */
std::vector<char*> pointersTo(std::vector<std::string>& texts) {
  std::vector<char*> pointers;
  for (std::size_t i = 0; i < texts.size(); ++i)
    pointers.push_back(&texts[i][0]);
  pointers.push_back(0);
  return pointers;
}

/**
 * @brief In the child: closes every descriptor above standard error.
 *        자식에서: 표준 에러보다 큰 디스크립터를 모두 닫는다.
 *
 * Without FD_CLOEXEC (F_SETFD is not allowed) the script would keep
 * other clients' sockets open (wiki concepts/cgi section 1). The numbers
 * are read from /dev/fd with the allowed opendir()/readdir() and closed
 * after closedir(), so the listing's own descriptor is gone by then.
 * FD_CLOEXEC가 없으면 (F_SETFD는 허용되지 않는다) 스크립트가 다른
 * 클라이언트 소켓을 열어 둔 채로 있게 된다 (위키 concepts/cgi 1절). 번호는
 * 허용 함수 opendir()/readdir()로 /dev/fd에서 읽고 closedir() 뒤에
 * 닫으므로, 목록을 읽는 데 쓴 디스크립터는 그때 이미 닫혀 있다.
 */
void closeInheritedDescriptors() {
  std::vector<int> open;
  DIR* directory = opendir(kDescriptorDirectory);
  if (directory == 0) return;

  for (dirent* entry = readdir(directory); entry != 0;
       entry = readdir(directory)) {
    unsigned long fd = 0;
    if (parseDecimal(entry->d_name, INT_MAX, fd) &&
        static_cast<int>(fd) >= kFirstOtherDescriptor)
      open.push_back(static_cast<int>(fd));
  }

  closedir(directory);
  for (std::size_t i = 0; i < open.size(); ++i) close(open[i]);
}

}  // namespace

CgiProcess::CgiProcess(const CgiLaunch& launch, const std::string& body,
                       const CgiServices& services)
    : launch_(launch),
      body_(body),
      registry_(services.registry),
      reaper_(services.reaper),
      pid_(-1),
      startedAt_(0),
      stdin_(0),
      stdout_(0) {}

// A child whose output ended may still be finishing, so it is only
// handed over; one that is still writing is killed with SIGKILL, which it
// cannot catch (wiki concepts/cgi section 5).
// 출력이 끝난 자식은 마무리 중일 수 있어 넘기기만 한다. 아직 쓰고 있는
// 자식은 잡을 수 없는 SIGKILL로 죽인다 (위키 concepts/cgi 5절).
CgiProcess::~CgiProcess() {
  delete stdin_;
  const bool done = isDone();
  delete stdout_;

  if (pid_ <= 0) return;
  if (!done) static_cast<void>(kill(pid_, SIGKILL));

  // An exception must not leave a destructor (it could end the server).
  // If the reaper cannot take the pid, the child is killed and waited for
  // here; SIGKILL keeps that wait short.
  // 소멸자에서 예외가 나가면 안 된다 (서버가 끝날 수 있다). reaper가 pid를
  // 받지 못하면 여기서 죽이고 기다린다. SIGKILL이라 기다림이 짧다.
  try {
    reaper_.adopt(pid_);
  } catch (const std::exception&) {
    static_cast<void>(kill(pid_, SIGKILL));
    int status = 0;
    static_cast<void>(waitpid(pid_, &status, 0));
  }
}

bool CgiProcess::start(std::time_t now) {
  int input[2];
  int output[2];
  if (!openPipes(input, output)) return false;

  // The parent's ends are owned from here on, so nothing after fork() can
  // leave them open.
  // 부모 쪽 끝은 여기서부터 소유하므로, fork() 뒤에 무엇이 실패해도 열린
  // 채 남지 않는다.
  stdin_ = new CgiStdin(input[1], body_, registry_);
  stdout_ = new CgiStdout(output[0], registry_);
  if (fcntl(input[1], F_SETFL, O_NONBLOCK) != 0 ||
      fcntl(output[0], F_SETFL, O_NONBLOCK) != 0) {
    close(input[0]);
    close(output[1]);
    return false;
  }

  // The child's std::exit() would print what is still buffered here a
  // second time (design decision 12).
  // 여기 버퍼에 남은 것을 자식의 std::exit()이 한 번 더 출력한다 (설계
  // 결정 12).
  std::cout.flush();
  std::cerr.flush();
  pid_ = fork();
  if (pid_ == 0) runChild(input[0], output[1]);
  close(input[0]);
  close(output[1]);
  if (pid_ < 0) return false;

  startedAt_ = now;
  watchPipes();
  return true;
}

// An empty body is closed at once, so the script reads EOF.
// 본문이 비었으면 바로 닫아 스크립트가 EOF를 읽게 한다.
void CgiProcess::watchPipes() {
  registry_.watch(stdout_);
  if (body_.empty())
    stdin_->close();
  else
    registry_.watch(stdin_);
}

bool CgiProcess::isDone() const { return stdout_ != 0 && stdout_->isClosed(); }

bool CgiProcess::failed() const { return stdout_ != 0 && stdout_->failed(); }

std::string CgiProcess::takeOutput() { return stdout_->takeData(); }

bool CgiProcess::hasTimedOut(std::time_t now) const {
  return !isDone() && now - startedAt_ > limits::kCgiTimeoutSeconds;
}

// Never returns. A failing step ends the child with std::exit(1); the
// parent then reads EOF without output and answers 502 (design 9). An
// exception (std::bad_alloc) must not unwind into the server's code, where
// the child would go on serving as a second server, so everything is
// caught. SIGPIPE was ignored by the server; the script gets the default
// back.
// 돌아오지 않는다. 실패한 단계는 자식을 std::exit(1)로 끝낸다. 부모는 출력
// 없이 EOF를 읽고 502로 답한다 (설계 9절). 예외(std::bad_alloc)가 서버
// 코드로 풀려 나가면 자식이 두 번째 서버처럼 계속 돌게 되므로 모두 잡는다.
// 서버가 무시한 SIGPIPE는 스크립트에 기본 동작으로 돌려준다.
void CgiProcess::runChild(int input, int output) {
  try {
    if (dup2(input, STDIN_FILENO) < 0 || dup2(output, STDOUT_FILENO) < 0)
      std::exit(EXIT_FAILURE);

    closeInheritedDescriptors();
    if (chdir(launch_.directory.c_str()) != 0) std::exit(EXIT_FAILURE);
    signal(SIGPIPE, SIG_DFL);

    std::vector<std::string> arguments;
    arguments.push_back(launch_.interpreter);
    arguments.push_back(launch_.scriptFile);
    const std::vector<char*> argv = pointersTo(arguments);
    const std::vector<char*> envp = pointersTo(launch_.environment);
    execve(argv[0], &argv[0], &envp[0]);
  } catch (...) {
  }
  std::exit(EXIT_FAILURE);
}
