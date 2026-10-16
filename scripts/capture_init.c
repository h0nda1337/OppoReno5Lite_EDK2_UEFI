/* SPDX-License-Identifier: BSD-2-Clause */
/* Minimal Linux PID 1 for a private hardware readout. This is NOT UEFI.
 * Uses stock kernel drivers; mounts proc/sys/dev/tmp/configfs only.
 * No block device is opened and no persistent partition is mounted or written.
 */
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/fb.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define GADGET "/config/usb_gadget/oppo_diag"
/* pyserial's Windows USB-parent traversal accepts word characters only. */
#define USB_IDENTITY "OPPOUEFIDIAG"
#define FILE_LIMIT (2 * 1024 * 1024)

static const char *files[] = {"runtime.dtb", "iomem.txt", "framebuffer.json", "battery.txt"};

static int put(const char *path, const char *value)
{
    int fd = open(path, O_WRONLY | O_CLOEXEC);
    size_t length = strlen(value);
    ssize_t result;
    if (fd < 0)
        return -1;
    result = write(fd, value, length);
    close(fd);
    return result == (ssize_t)length ? 0 : -1;
}

static int battery(void)
{
    char text[32], *end;
    long value;
    int fd = open("/sys/class/power_supply/battery/capacity", O_RDONLY | O_CLOEXEC);
    ssize_t count;
    if (fd < 0)
        return -1;
    count = read(fd, text, sizeof(text) - 1);
    close(fd);
    if (count <= 0)
        return -1;
    text[count] = 0;
    errno = 0;
    value = strtol(text, &end, 10);
    while (*end == '\n' || *end == '\r' || *end == ' ')
        end++;
    return errno || end == text || *end || value < 0 || value > 100 ? -1 : (int)value;
}

static int copy_file(const char *source, const char *dest)
{
    char buffer[4096];
    size_t total = 0;
    ssize_t count;
    int in = open(source, O_RDONLY | O_CLOEXEC), out;
    if (in < 0)
        return -1;
    out = open(dest, O_CREAT | O_TRUNC | O_WRONLY | O_CLOEXEC, 0600);
    if (out < 0) {
        close(in);
        return -1;
    }
    while ((count = read(in, buffer, sizeof(buffer))) > 0) {
        total += (size_t)count;
        if (total > FILE_LIMIT || write(out, buffer, count) != count) {
            close(in);
            close(out);
            return -1;
        }
    }
    close(in);
    close(out);
    return count < 0 ? -1 : 0;
}

static int device_node(const char *sysfs, const char *path)
{
    unsigned major_id, minor_id;
    FILE *stream = fopen(sysfs, "r");
    if (!stream)
        return -1;
    if (fscanf(stream, "%u:%u", &major_id, &minor_id) != 2) {
        fclose(stream);
        return -1;
    }
    fclose(stream);
    if (mknod(path, S_IFCHR | 0600, makedev(major_id, minor_id)) && errno != EEXIST)
        return -1;
    return 0;
}

static int framebuffer(void)
{
    struct fb_fix_screeninfo fix = {0};
    struct fb_var_screeninfo var = {0};
    FILE *report;
    int fd;
    uint8_t *mapping;
    uint64_t last;
    unsigned x, y;
    device_node("/sys/class/graphics/fb0/dev", "/dev/fb0");
    fd = open("/dev/fb0", O_RDWR | O_CLOEXEC);
    if (fd < 0 || ioctl(fd, FBIOGET_FSCREENINFO, &fix) ||
        ioctl(fd, FBIOGET_VSCREENINFO, &var)) {
        if (fd >= 0)
            close(fd);
        return -1;
    }
    report = fopen("/tmp/framebuffer.json", "w");
    if (!report) {
        close(fd);
        return -1;
    }
    fprintf(report, "{\"schema_version\":1,\"source\":\"linux-fb-ioctl\","
            "\"physical_base\":\"0x%" PRIx64 "\",\"memory_size\":%u,"
            "\"stride_bytes\":%u,\"width\":%u,\"height\":%u,"
            "\"virtual_width\":%u,\"virtual_height\":%u,"
            "\"xoffset\":%u,\"yoffset\":%u,\"bits_per_pixel\":%u,"
            "\"red\":[%u,%u],\"green\":[%u,%u],\"blue\":[%u,%u],"
            "\"alpha\":[%u,%u]}\n",
            (uint64_t)fix.smem_start, fix.smem_len, fix.line_length,
            var.xres, var.yres, var.xres_virtual, var.yres_virtual,
            var.xoffset, var.yoffset, var.bits_per_pixel,
            var.red.offset, var.red.length, var.green.offset, var.green.length,
            var.blue.offset, var.blue.length, var.transp.offset, var.transp.length);
    fclose(report);
    /* Small green/white status bar through the stock Linux framebuffer driver.
     * This is a Linux diagnostic marker, never evidence of bare-metal UEFI.
     */
    last = ((uint64_t)var.yoffset + 128) * fix.line_length +
           ((uint64_t)var.xoffset + var.xres) * 4;
    if (var.bits_per_pixel == 32 && var.xres >= 64 && var.xres <= 4096 &&
        var.yres >= 128 && var.yoffset <= 16384 && var.xoffset <= 4096 &&
        fix.line_length <= 65536 && fix.line_length % 4 == 0 &&
        fix.line_length >= ((uint64_t)var.xoffset + var.xres) * 4 &&
        var.green.length == 8 && var.green.offset <= 24 &&
        fix.smem_len <= 256 * 1024 * 1024 && last <= fix.smem_len) {
        mapping = mmap(NULL, fix.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (mapping != MAP_FAILED) {
            for (y = 80; y < 128; y++) {
                uint32_t *line = (uint32_t *)(mapping + ((uint64_t)var.yoffset + y) *
                                            fix.line_length + (uint64_t)var.xoffset * 4);
                for (x = 0; x < var.xres; x++)
                    line[x] = x < var.xres / 2 ? (0xffU << var.green.offset) : 0xffffffffU;
            }
            msync(mapping, fix.smem_len, MS_SYNC);
            munmap(mapping, fix.smem_len);
        }
    }
    close(fd);
    return 0;
}

static int setup_usb(void)
{
    DIR *dir;
    struct dirent *item;
    char udc[128] = {0};
    mkdir("/config", 0755);
    if (mount("configfs", "/config", "configfs", 0, NULL) && errno != EBUSY)
        return -1;
    mkdir(GADGET, 0755);
    if (put(GADGET "/idVendor", "0x1d6b") || put(GADGET "/idProduct", "0x0104") ||
        put(GADGET "/bDeviceClass", "2"))
        return -1;
    mkdir(GADGET "/strings/0x409", 0755);
    if (put(GADGET "/strings/0x409/serialnumber", USB_IDENTITY) ||
        put(GADGET "/strings/0x409/manufacturer", "OPPO EDK2 research") ||
        put(GADGET "/strings/0x409/product", "Linux hardware readout"))
        return -1;
    mkdir(GADGET "/configs/c.1", 0755);
    mkdir(GADGET "/configs/c.1/strings/0x409", 0755);
    put(GADGET "/configs/c.1/strings/0x409/configuration", "Readout ACM");
    put(GADGET "/configs/c.1/MaxPower", "500");
    if (mkdir(GADGET "/functions/acm.usb0", 0755) && errno != EEXIST)
        return -1;
    if (symlink(GADGET "/functions/acm.usb0", GADGET "/configs/c.1/acm.usb0") && errno != EEXIST)
        return -1;
    dir = opendir("/sys/class/udc");
    if (!dir)
        return -1;
    while ((item = readdir(dir))) {
        if (item->d_name[0] != '.' && strlen(item->d_name) < sizeof(udc)) {
            strcpy(udc, item->d_name);
            break;
        }
    }
    closedir(dir);
    if (!udc[0] || put(GADGET "/UDC", udc))
        return -1;
    put("/sys/class/udc/musb-hdrc/device/cmode", "1");
    return device_node("/sys/class/tty/ttyGS0/dev", "/dev/ttyGS0");
}

static int send_bytes(int fd, const void *data, size_t size)
{
    const char *bytes = data;
    while (size) {
        struct pollfd poll_fd = {fd, POLLOUT, 0};
        ssize_t written;
        if (poll(&poll_fd, 1, 5000) <= 0)
            return -1;
        written = write(fd, bytes, size);
        if (written < 0 && (errno == EINTR || errno == EAGAIN))
            continue;
        if (written <= 0)
            return -1;
        size -= written;
        bytes += written;
    }
    return 0;
}

static int send_snapshot(int fd)
{
    unsigned i;
    char buffer[4096], path[128], header[160];
    if (send_bytes(fd, "OPPO-DIAG/1 BEGIN\n", 18))
        return -1;
    for (i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        struct stat info;
        ssize_t count;
        int in, length;
        snprintf(path, sizeof(path), "/tmp/%s", files[i]);
        in = open(path, O_RDONLY | O_CLOEXEC);
        if (in < 0) {
            length = snprintf(header, sizeof(header), "FILE %s 0\n\n", files[i]);
            if (send_bytes(fd, header, (size_t)length))
                return -1;
            continue;
        }
        if (fstat(in, &info) || info.st_size < 0 || info.st_size > FILE_LIMIT) {
            if (in >= 0)
                close(in);
            return -1;
        }
        length = snprintf(header, sizeof(header), "FILE %s %jd\n", files[i], (intmax_t)info.st_size);
        if (send_bytes(fd, header, (size_t)length)) {
            close(in);
            return -1;
        }
        while ((count = read(in, buffer, sizeof(buffer))) > 0) {
            if (send_bytes(fd, buffer, (size_t)count)) {
                close(in);
                return -1;
            }
        }
        close(in);
        if (count < 0 || send_bytes(fd, "\n", 1))
            return -1;
    }
    return send_bytes(fd, "OPPO-DIAG/1 END\n", 16);
}

int main(void)
{
    struct termios term;
    char command[32] = {0};
    size_t used = 0;
    time_t deadline;
    int fd = -1, level = -1, i;
    /* An accidental host invocation must never mount, reconfigure USB or reboot. */
    if (getpid() != 1) {
        fprintf(stderr, "capture-init requires Linux PID 1; no operation performed\n");
        return 2;
    }
    signal(SIGCHLD, SIG_IGN);
    mkdir("/dev", 0755);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
    mkdir("/proc", 0755);
    mkdir("/sys", 0755);
    mkdir("/tmp", 0700);
    mount("proc", "/proc", "proc", MS_NOSUID | MS_NODEV, NULL);
    mount("sysfs", "/sys", "sysfs", MS_NOSUID | MS_NODEV, NULL);
    mount("tmpfs", "/tmp", "tmpfs", MS_NOSUID | MS_NODEV, "size=16m,mode=0700");
    for (i = 0; i < 20 && level < 0; i++) {
        level = battery();
        if (level < 0)
            sleep(1);
    }
    if (level < 20)
        goto return_android;
    copy_file("/sys/class/power_supply/battery/capacity", "/tmp/battery.txt");
    copy_file("/sys/firmware/fdt", "/tmp/runtime.dtb");
    copy_file("/proc/iomem", "/tmp/iomem.txt");
    framebuffer();
    for (i = 0; i < 20; i++) {
        if (!setup_usb())
            break;
        sleep(1);
    }
    fd = open("/dev/ttyGS0", O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0)
        goto return_android;
    if (!tcgetattr(fd, &term)) {
        cfmakeraw(&term);
        cfsetispeed(&term, B115200);
        cfsetospeed(&term, B115200);
        term.c_cflag |= CLOCAL | CREAD;
        tcsetattr(fd, TCSANOW, &term);
    }
    deadline = time(NULL) + 180;
    while (time(NULL) < deadline) {
        struct pollfd poll_fd = {fd, POLLIN, 0};
        char value;
        if (poll(&poll_fd, 1, 1000) <= 0 || read(fd, &value, 1) != 1)
            continue;
        if (value == '\n') {
            command[used] = 0;
            if (!strcmp(command, "READ") && battery() >= 20)
                send_snapshot(fd);
            else if (!strcmp(command, "REBOOT") && battery() >= 20)
                break;
            used = 0;
        } else if (used < sizeof(command) - 1 && value != '\r') {
            command[used++] = value;
        }
    }
return_android:
    if (fd >= 0)
        close(fd);
    sync();
    reboot(RB_AUTOBOOT);
    for (;;)
        pause();
}
