#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/gpio.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

static int sleep_ms(unsigned long milliseconds) {
    struct timespec delay = {
        .tv_sec = (time_t)(milliseconds / 1000UL),
        .tv_nsec = (long)((milliseconds % 1000UL) * 1000000UL)
    };
    while (nanosleep(&delay, &delay) < 0) {
        if (errno != EINTR) return -1;
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 3 || argc > 5) {
        fprintf(stderr, "Usage: %s /dev/gpiochipN line_offset [cycles=10] [period_ms=200]\n", argv[0]);
        return 2;
    }
    char* end = NULL;
    const unsigned long offset = strtoul(argv[2], &end, 0);
    if (end == argv[2] || *end != '\0' || offset > UINT32_MAX) {
        fprintf(stderr, "Invalid line offset\n");
        return 2;
    }
    end = NULL;
    const unsigned long cycles = argc >= 4 ? strtoul(argv[3], &end, 0) : 10UL;
    if ((argc >= 4 && (end == argv[3] || *end != '\0')) || cycles == 0UL || cycles > 100000UL) {
        fprintf(stderr, "Invalid cycle count\n");
        return 2;
    }
    end = NULL;
    const unsigned long period_ms = argc == 5 ? strtoul(argv[4], &end, 0) : 200UL;
    if ((argc == 5 && (end == argv[4] || *end != '\0')) || period_ms < 2UL || period_ms > 60000UL) {
        fprintf(stderr, "period_ms must be 2..60000\n");
        return 2;
    }

    const int chip = open(argv[1], O_RDONLY | O_CLOEXEC);
    if (chip < 0) {
        perror("open gpiochip");
        return 1;
    }

    struct gpio_v2_line_request request;
    memset(&request, 0, sizeof(request));
    request.offsets[0] = (uint32_t)offset;
    request.num_lines = 1U;
    request.config.flags = GPIO_V2_LINE_FLAG_OUTPUT;
    request.config.num_attrs = 1U;
    request.config.attrs[0].attr.id = GPIO_V2_LINE_ATTR_ID_OUTPUT_VALUES;
    request.config.attrs[0].attr.values = 0U;
    request.config.attrs[0].mask = 1U;
    (void)snprintf(request.consumer, sizeof(request.consumer), "%s", "jetson-gpio-practice");
    if (ioctl(chip, GPIO_V2_GET_LINE_IOCTL, &request) < 0) {
        perror("GPIO_V2_GET_LINE_IOCTL");
        close(chip);
        return 1;
    }
    close(chip);

    int result = 0;
    for (unsigned long index = 0; index < cycles * 2UL; ++index) {
        struct gpio_v2_line_values values = {
            .bits = index % 2UL == 0UL ? 1U : 0U,
            .mask = 1U
        };
        if (ioctl(request.fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &values) < 0) {
            perror("GPIO_V2_LINE_SET_VALUES_IOCTL");
            result = 1;
            break;
        }
        printf("line=%lu value=%u\n", offset, values.bits != 0U ? 1U : 0U);
        if (sleep_ms(period_ms / 2UL) < 0) {
            perror("nanosleep");
            result = 1;
            break;
        }
    }

    struct gpio_v2_line_values safe = {.bits = 0U, .mask = 1U};
    if (ioctl(request.fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &safe) < 0) {
        perror("set GPIO safe low");
        result = 1;
    }
    close(request.fd);
    return result;
}
