#define _DEFAULT_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static speed_t baud_flag(long baud) {
    switch (baud) {
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
        case 460800: return B460800;
        case 921600: return B921600;
        default: return (speed_t)0;
    }
}

static int write_all(int fd, const unsigned char* data, size_t length) {
    size_t sent = 0;
    while (sent < length) {
        const ssize_t count = write(fd, data + sent, length - sent);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return -1;
        sent += (size_t)count;
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2 || argc > 4) {
        fprintf(stderr, "Usage: %s /dev/ttyTHSX [baud=115200] [message]\n", argv[0]);
        return 2;
    }

    char* end = NULL;
    const long baud = argc >= 3 ? strtol(argv[2], &end, 10) : 115200L;
    if ((argc >= 3 && (end == argv[2] || *end != '\0')) || baud_flag(baud) == (speed_t)0) {
        fprintf(stderr, "Unsupported baud rate\n");
        return 2;
    }
    const char* message = argc == 4 ? argv[3] : "jetson-uart-loopback";
    const size_t message_length = strlen(message);
    if (message_length == 0U || message_length > 4096U) {
        fprintf(stderr, "Message length must be 1..4096 bytes\n");
        return 2;
    }

    const int fd = open(argv[1], O_RDWR | O_NOCTTY | O_CLOEXEC | O_NONBLOCK);
    if (fd < 0) {
        perror("open UART");
        return 1;
    }

    struct termios config;
    if (tcgetattr(fd, &config) < 0) {
        perror("tcgetattr");
        close(fd);
        return 1;
    }
    cfmakeraw(&config);
    config.c_cflag |= CLOCAL | CREAD;
    config.c_cflag &= ~(PARENB | CSTOPB | CRTSCTS);
    config.c_cflag = (config.c_cflag & (tcflag_t)~CSIZE) | CS8;
    if (cfsetispeed(&config, baud_flag(baud)) < 0 || cfsetospeed(&config, baud_flag(baud)) < 0 ||
        tcsetattr(fd, TCSANOW, &config) < 0) {
        perror("configure UART");
        close(fd);
        return 1;
    }
    if (tcflush(fd, TCIOFLUSH) < 0 || write_all(fd, (const unsigned char*)message, message_length) < 0 ||
        tcdrain(fd) < 0) {
        perror("UART transmit");
        close(fd);
        return 1;
    }

    unsigned char received[4096];
    size_t total = 0;
    while (total < message_length) {
        struct pollfd event = {.fd = fd, .events = POLLIN, .revents = 0};
        const int ready = poll(&event, 1, 2000);
        if (ready < 0 && errno == EINTR) continue;
        if (ready <= 0) {
            fprintf(stderr, "UART receive timeout (%zu/%zu bytes)\n", total, message_length);
            close(fd);
            return 1;
        }
        const ssize_t count = read(fd, received + total, message_length - total);
        if (count < 0 && (errno == EINTR || errno == EAGAIN)) continue;
        if (count <= 0) {
            perror("UART receive");
            close(fd);
            return 1;
        }
        total += (size_t)count;
    }

    const bool match = memcmp(message, received, message_length) == 0;
    printf("baud=%ld sent=%zu received=%zu match=%s\n", baud, message_length, total, match ? "yes" : "no");
    close(fd);
    return match ? 0 : 1;
}
