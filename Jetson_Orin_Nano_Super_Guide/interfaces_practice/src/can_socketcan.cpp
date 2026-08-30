#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {
class Socket {
public:
    Socket() : fd_(::socket(PF_CAN, SOCK_RAW | SOCK_CLOEXEC, CAN_RAW)) {
        if (fd_ < 0) throw std::runtime_error(std::string("socket: ") + std::strerror(errno));
    }
    ~Socket() { ::close(fd_); }
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    int get() const { return fd_; }
private:
    int fd_;
};

unsigned long parse_number(const char* text, unsigned long maximum, const char* label) {
    std::size_t used = 0;
    const unsigned long value = std::stoul(text, &used, 0);
    if (used != std::strlen(text) || value > maximum) throw std::invalid_argument(std::string("invalid ") + label);
    return value;
}

void bind_interface(int fd, const char* name) {
    ifreq request{};
    if (std::strlen(name) >= IFNAMSIZ) throw std::invalid_argument("CAN interface name is too long");
    std::strncpy(request.ifr_name, name, IFNAMSIZ - 1);
    if (::ioctl(fd, SIOCGIFINDEX, &request) < 0) throw std::runtime_error(std::string("SIOCGIFINDEX: ") + std::strerror(errno));
    sockaddr_can address{};
    address.can_family = AF_CAN;
    address.can_ifindex = request.ifr_ifindex;
    if (::bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        throw std::runtime_error(std::string("bind: ") + std::strerror(errno));
    }
}

canid_t make_id(unsigned long raw) {
    if (raw <= CAN_SFF_MASK) return static_cast<canid_t>(raw);
    if (raw <= CAN_EFF_MASK) return static_cast<canid_t>(raw) | CAN_EFF_FLAG;
    throw std::invalid_argument("CAN ID must fit 11 or 29 bits");
}
}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage:\n  " << argv[0] << " can0 recv\n  " << argv[0]
                  << " can0 send|sendfd CAN_ID [hex_byte ...]\n";
        return 2;
    }

    try {
        Socket socket;
        const std::string command = argv[2];
        const bool fd_mode = command == "sendfd";
        if (fd_mode || command == "recv") {
            const int enable = 1;
            if (::setsockopt(socket.get(), SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &enable, sizeof(enable)) < 0) {
                throw std::runtime_error(std::string("enable CAN FD: ") + std::strerror(errno));
            }
        }
        bind_interface(socket.get(), argv[1]);

        if (command == "recv") {
            pollfd event{socket.get(), POLLIN, 0};
            const int ready = ::poll(&event, 1, 5000);
            if (ready == 0) throw std::runtime_error("receive timeout after 5 seconds");
            if (ready < 0) throw std::runtime_error(std::string("poll: ") + std::strerror(errno));
            canfd_frame frame{};
            const ssize_t count = ::read(socket.get(), &frame, sizeof(frame));
            if (count != CAN_MTU && count != CANFD_MTU) throw std::runtime_error("unexpected CAN frame size");
            const canid_t id = frame.can_id & (frame.can_id & CAN_EFF_FLAG ? CAN_EFF_MASK : CAN_SFF_MASK);
            std::cout << "id=0x" << std::hex << id << " data=";
            for (std::uint8_t index = 0; index < frame.len; ++index) {
                std::cout << " " << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(frame.data[index]);
            }
            std::cout << std::dec << '\n';
            return 0;
        }

        if ((command != "send" && !fd_mode) || argc < 4) throw std::invalid_argument("invalid command");
        const std::size_t payload_size = static_cast<std::size_t>(argc - 4);
        const std::size_t maximum = fd_mode ? CANFD_MAX_DLEN : CAN_MAX_DLEN;
        if (payload_size > maximum) throw std::invalid_argument("payload exceeds frame capacity");

        canfd_frame frame{};
        frame.can_id = make_id(parse_number(argv[3], CAN_EFF_MASK, "CAN ID"));
        frame.len = static_cast<__u8>(payload_size);
        for (std::size_t index = 0; index < payload_size; ++index) {
            frame.data[index] = static_cast<__u8>(parse_number(argv[index + 4], 0xffUL, "data byte"));
        }
        const std::size_t frame_size = fd_mode ? CANFD_MTU : CAN_MTU;
        const ssize_t written = ::write(socket.get(), &frame, frame_size);
        if (written != static_cast<ssize_t>(frame_size)) throw std::runtime_error(std::string("write CAN: ") + std::strerror(errno));
        std::cout << "sent id=0x" << std::hex << (frame.can_id & CAN_EFF_MASK) << std::dec
                  << " bytes=" << payload_size << " fd=" << (fd_mode ? "yes" : "no") << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
}
