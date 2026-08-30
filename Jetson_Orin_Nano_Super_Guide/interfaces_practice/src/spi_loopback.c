#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

int main(int argc, char** argv) {
    if (argc < 2 || argc > 4) {
        fprintf(stderr, "Usage: %s /dev/spidevB.C [speed_hz=500000] [bytes=32]\n", argv[0]);
        return 2;
    }

    char* end = NULL;
    const unsigned long speed_value = argc >= 3 ? strtoul(argv[2], &end, 0) : 500000UL;
    if ((argc >= 3 && (end == argv[2] || *end != '\0')) || speed_value == 0UL || speed_value > UINT32_MAX) {
        fprintf(stderr, "Invalid speed\n");
        return 2;
    }
    end = NULL;
    const unsigned long length_value = argc == 4 ? strtoul(argv[3], &end, 0) : 32UL;
    if ((argc == 4 && (end == argv[3] || *end != '\0')) || length_value == 0UL || length_value > 4096UL) {
        fprintf(stderr, "Length must be 1..4096\n");
        return 2;
    }
    const uint32_t speed = (uint32_t)speed_value;
    const size_t length = (size_t)length_value;

    uint8_t* tx = calloc(length, sizeof(*tx));
    uint8_t* rx = calloc(length, sizeof(*rx));
    if (tx == NULL || rx == NULL) {
        perror("calloc");
        free(tx);
        free(rx);
        return 1;
    }
    for (size_t index = 0; index < length; ++index) tx[index] = (uint8_t)(0xA5U ^ (uint8_t)index);

    const int fd = open(argv[1], O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        perror("open SPI");
        free(tx);
        free(rx);
        return 1;
    }
    uint8_t mode = SPI_MODE_0;
    uint8_t bits = 8U;
    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0 || ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
        ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        perror("configure SPI");
        close(fd);
        free(tx);
        free(rx);
        return 1;
    }

    struct spi_ioc_transfer transfer;
    memset(&transfer, 0, sizeof(transfer));
    transfer.tx_buf = (uintptr_t)tx;
    transfer.rx_buf = (uintptr_t)rx;
    transfer.len = (uint32_t)length;
    transfer.speed_hz = speed;
    transfer.bits_per_word = bits;
    if (ioctl(fd, SPI_IOC_MESSAGE(1), &transfer) < 0) {
        perror("SPI transfer");
        close(fd);
        free(tx);
        free(rx);
        return 1;
    }

    const int match = memcmp(tx, rx, length) == 0;
    printf("mode=0 speed=%u bytes=%zu match=%s\n", speed, length, match ? "yes" : "no");
    if (!match) {
        const size_t shown = length < 16U ? length : 16U;
        for (size_t index = 0; index < shown; ++index) {
            printf("[%zu] tx=%02x rx=%02x%s", index, tx[index], rx[index], index + 1U == shown ? "\n" : "  ");
        }
    }
    close(fd);
    free(tx);
    free(rx);
    return match ? 0 : 1;
}
