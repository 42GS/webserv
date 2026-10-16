#include <iostream>

namespace {

const char* const kDefaultConfigPath = "conf/default.conf";
const int kExitSuccess = 0;
const int kExitUsageError = 1;

}  // namespace

int main(int argc, char** argv)
{
    if (argc > 2) {
        std::cerr << "usage: " << argv[0] << " [configuration file]" << std::endl;
        return kExitUsageError;
    }
    const char* configPath = (argc == 2) ? argv[1] : kDefaultConfigPath;
    std::cout << "webserv: configuration file: " << configPath << std::endl;
    return kExitSuccess;
}
