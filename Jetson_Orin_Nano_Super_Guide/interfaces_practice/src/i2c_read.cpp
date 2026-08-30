#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <stdexcept>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>
#include <vector>

namespace {
unsigned long parse_number(const char* text, unsigned long maximum, const char* name) {
    std::size_t used = 0;
    const unsigned long value = std::stoul(text, &used, 0);
    if (used != std::strlen(text) || value > maximum) {
        throw std::invalid_argument(std::string(name) + " is out of range");
    }
    return value;
}

class FileDescriptor {
public:
    explicit FileDescriptor(const char* path) : fd_(::open(path, O_RDWR | O_CLOEXEC)) {
        if (fd_ < 0) {
            throw std::runtime_error(std::string("open ") + path + ": " + std::strerror(errno));
        }
    }
    ~FileDescriptor() { ::close(fd_); }
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;
    int get() const { return fd_; }
private:
    int fd_;
};
}  // namespace

int main(int argc, char** argv) {
    if (argc < 4 || argc > 5) {
        std::cerr << "Usage: " << argv[0] << " /dev/i2c-X 0xADDR 0xREG [length=1]\n";
        return 2;
    }

    try {
        const auto address = static_cast<std::uint16_t>(parse_number(argv[2], 0x7fUL, "7-bit address"));
        auto reg = static_cast<std::uint8_t>(parse_number(argv[3], 0xffUL, "8-bit register"));
        const auto length = static_cast<std::size_t>(argc == 5 ? parse_number(argv[4], 256UL, "length") : 1UL);
        if (length == 0U) {
            throw std::invalid_argument("length must be at least 1");
        }

        FileDescriptor bus(argv[1]);
        std::vector<std::uint8_t> data(length, 0U);
        i2c_msg messages[2]{};
        messages[0].addr = address;
        messages[0].flags = 0;
        messages[0].len = 1;
        messages[0].buf = &reg;
        messages[1].addr = address;
        messages[1].flags = I2C_M_RD;
        messages[1].len = static_cast<__u16>(data.size());
        messages[1].buf = data.data();

        i2c_rdwr_ioctl_data transfer{};
        transfer.msgs = messages;
        transfer.nmsgs = 2;
        if (::ioctl(bus.get(), I2C_RDWR, &transfer) < 0) {
            throw std::runtime_error(std::string("I2C_RDWR: ") + std::strerror(errno));
        }

        std::cout << "addr=0x" << std::hex << std::setw(2) << std::setfill('0') << address
                  << " reg=0x" << std::setw(2) << static_cast<unsigned int>(reg) << " data=";
        for (const auto byte : data) {
            std::cout << " " << std::setw(2) << static_cast<unsigned int>(byte);
        }
        std::cout << std::dec << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
}

