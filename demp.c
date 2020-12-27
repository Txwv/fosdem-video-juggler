/*
 * Copyright (c) 2019-2020 Luc Verhaegen <libv@skynet.be>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdbool.h>

#include <linux/videodev2.h>

#if defined(__LINUX_VIDEODEV2_H) && !defined(V4L2_PIX_FMT_R8_G8_B8)
/*
 * This definition might be out of sync with v4l2 though.
 */
#warning "V4L2_PIX_FMT_R8_G8_B8 undefined. Working around it."
#define V4L2_PIX_FMT_R8_G8_B8 v4l2_fourcc('P', 'R', 'G', 'B') /* 24bit planar RGB */
#endif

#define DRIVER_NAME "sun4i_demp"

static int demp_fd;

static int
demp_device_open_and_verify(int number)
{
	struct v4l2_capability capability[1] = {{{ 0 }}};
	char filename[128] = "";
	bool has_prgb = false, has_nv12 = false;
	int fd, ret, i;

	ret = snprintf(filename, sizeof(filename), "/dev/video%d", number);
	if (ret < 0) {
		fprintf(stderr, "Error: %s(%d):snprintf() failed: %s",
			__func__, number, strerror(-ret));
		return ret;
	}

	fd = open(filename, O_RDWR);
	if (fd < 0) {
		if ((errno == ENODEV) || (errno == ENOENT)) {
			return 0; /* next! */
		} else {
			fprintf(stderr, "Error: %s():open(%s): %s\n",
				__func__, filename, strerror(errno));
			return fd;
		}
	}

	ret = ioctl(fd, VIDIOC_QUERYCAP, capability);
	if (ret < 0) {
		fprintf(stderr, "Error: %s():ioctl(%s, QUERYCAP): %s\n",
			__func__, filename, strerror(errno));
		close(fd);
		return ret;
	}

	if (strcmp(DRIVER_NAME, (const char *) capability->driver)) {
		/* not our driver */
		close(fd);
		return 0; /* next! */
	}

	if (!(capability->device_caps & V4L2_CAP_VIDEO_M2M_MPLANE)) {
		fprintf(stderr, "Error: %s(): %s is not VIDEO_M2M_MPLANE.\n",
			__func__, filename);
		close(fd);
		return -EINVAL;
	}

	printf("Input Formats (aka VIDEO_OUTPUT):\n");
	for (i = 0; ; i++) {
		struct v4l2_fmtdesc descriptor[1] = {{
			.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
			.index = i,
		}};

		ret = ioctl(fd, VIDIOC_ENUM_FMT, descriptor);
		if (ret) {
			if (errno == EINVAL)
				break;

			fprintf(stderr,
				"Error: %s():ioctl(ENUM_FMT(input)): %s\n",
				__func__, strerror(errno));
			close(fd);
			return errno;
		}

		if (descriptor->pixelformat == V4L2_PIX_FMT_R8_G8_B8)
			has_prgb = true;

		printf("  %C%C%C%C.\n",
		       (descriptor->pixelformat >> 0) & 0xFF,
		       (descriptor->pixelformat >> 8) & 0xFF,
		       (descriptor->pixelformat >> 16) & 0xFF,
		       (descriptor->pixelformat >> 24) & 0xFF);
	}

	if (!has_prgb) {
		fprintf(stderr, "Error: %s(): missing R8_G8_B8 format.\n",
			__func__);
		close(fd);
		return -EINVAL;
	}

	printf("Output Formats (aka VIDEO_CAPTURE):\n");
	for (i = 0; ; i++) {
		struct v4l2_fmtdesc descriptor[1] = {{
			.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
			.index = i,
		}};

		ret = ioctl(fd, VIDIOC_ENUM_FMT, descriptor);
		if (ret) {
			if (errno == EINVAL)
				break;

			fprintf(stderr,
				"Error: %s():ioctl(ENUM_FMT(output)): %s\n",
				__func__, strerror(errno));
			close(fd);
			return errno;
		}

		if (descriptor->pixelformat == V4L2_PIX_FMT_NV12)
			has_nv12 = true;

		printf("  %C%C%C%C.\n",
		       (descriptor->pixelformat >> 0) & 0xFF,
		       (descriptor->pixelformat >> 8) & 0xFF,
		       (descriptor->pixelformat >> 16) & 0xFF,
		       (descriptor->pixelformat >> 24) & 0xFF);
	}

	if (!has_nv12) {
		fprintf(stderr, "Error: %s(): missing NV12 format.\n",
			__func__);
		close(fd);
		return -EINVAL;
	}

	printf("Found %s driver as %s.\n", DRIVER_NAME, filename);

	return fd;
}

static int
demp_device_find(void)
{
	int i;

	for (i = 0; i < 16; i++) {
		int fd;

		fd = demp_device_open_and_verify(i);
		if (fd)
			return fd;
	}

	fprintf(stderr, "Error: %s: unable to find /dev/videoX node for "
		"\"%s\"\n", __func__, DRIVER_NAME);
	return -ENODEV;
}

int main(int argc, char *argv[])
{
	demp_fd = demp_device_find();
	if (demp_fd < 0)
		return -demp_fd;

	return 0;
}
