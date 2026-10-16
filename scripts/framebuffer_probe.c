/* SPDX-License-Identifier: BSD-2-Clause */
/* Linux recovery readout: only FBIOGET ioctls, never mmap or display writes. */
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/fb.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    struct fb_fix_screeninfo fix = {0};
    struct fb_var_screeninfo var = {0};
    int fd;
    if (argc != 2) {
        fprintf(stderr, "Usage: framebuffer-probe /dev/graphics/fb0\n");
        return 2;
    }
    fd = open(argv[1], O_RDONLY | O_CLOEXEC);
    if (fd < 0 || ioctl(fd, FBIOGET_FSCREENINFO, &fix) ||
        ioctl(fd, FBIOGET_VSCREENINFO, &var)) {
        fprintf(stderr, "Framebuffer read failed: %s\n", strerror(errno));
        if (fd >= 0)
            close(fd);
        return 1;
    }
    close(fd);
    printf("{\"schema_version\":1,\"source\":\"linux-fb-ioctl\","
           "\"physical_base\":\"0x%" PRIx64 "\",\"memory_size\":%u,"
           "\"stride_bytes\":%u,\"width\":%u,\"height\":%u,"
           "\"virtual_width\":%u,\"virtual_height\":%u,"
           "\"xoffset\":%u,\"yoffset\":%u,\"bits_per_pixel\":%u,"
           "\"red\":[%u,%u],\"green\":[%u,%u],\"blue\":[%u,%u],"
           "\"alpha\":[%u,%u],\"display_memory_written\":false}\n",
           (uint64_t)fix.smem_start, fix.smem_len, fix.line_length,
           var.xres, var.yres, var.xres_virtual, var.yres_virtual,
           var.xoffset, var.yoffset, var.bits_per_pixel,
           var.red.offset, var.red.length, var.green.offset, var.green.length,
           var.blue.offset, var.blue.length, var.transp.offset, var.transp.length);
    return 0;
}
