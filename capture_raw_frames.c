/*
 *  V4L2 video capture example
 *
 *  This program can be used and distributed without restrictions.
 *
 *      This program is provided with the V4L2 API
 * see http://linuxtv.org/docs.php for more information
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <getopt.h>             /* getopt_long() */

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>

#include <aalib.h>
#include <inttypes.h>

#define CLEAR(x) memset(&(x), 0, sizeof(x))

struct buffer
{
	void   *start;
	size_t  length;
};

static char            *dev_name = "/dev/video0";
static int              fd = -1;
struct buffer          *buffers;
static unsigned int     n_buffers;
static int              force_format = 0;
static int              v4l2_width;
static int              v4l2_height;

static aa_context      *ctx;
static aa_savedata      aa_save;
static aa_renderparams *params;
static float            xstep;
static float            ystep;
static int              aa_width;
static int              aa_height;

static void errno_exit(const char *s)
{
	fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
	exit(EXIT_FAILURE);
}

static int xioctl(int fh, int request, void *arg)
{
	int r;

	do
		r = ioctl(fh, request, arg);
	while (-1 == r && EINTR == errno);

	return r;
}

static int v4l2_index(int x, int y)
{
	int idx = (float) y * ystep;
	idx *= v4l2_width;
	idx += (float) x * xstep;
	idx *= 2;
	return idx;
}

static void process_image(const uint8_t *p, int size)
{
	for (int y = 0; y < aa_height; y++)
		for (int x = 0; x < aa_width; x++)
			aa_putpixel(ctx, x, y, p[v4l2_index(x, y)]);
	aa_render(ctx, params, 0, 0, aa_scrwidth(ctx), aa_scrheight(ctx));
	aa_flush(ctx);
	printf("\f");
}

static int read_frame(void)
{
	struct v4l2_buffer buf;

	CLEAR(buf);

	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf))
	{
		switch (errno) {
		case EAGAIN:
			return 0;

		case EIO:
			/* Could ignore EIO, see spec. */
			/* fall through */
		default:
			errno_exit("VIDIOC_DQBUF");
		}
	}

	assert(buf.index < n_buffers);

	process_image(buffers[buf.index].start, buf.bytesused);

	if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
		errno_exit("VIDIOC_QBUF");

	return 1;
}

static void mainloop(void)
{
	while (42)
	{
		for (;;)
		{
			fd_set fds;
			struct timeval tv;
			int r;

			FD_ZERO(&fds);
			FD_SET(fd, &fds);

			/* Timeout. */
			tv.tv_sec = 2;
			tv.tv_usec = 0;

			r = select(fd + 1, &fds, NULL, NULL, &tv);

			if (-1 == r)
			{
				if (EINTR == errno)
					continue;
				errno_exit("select");
			}

			if (0 == r)
			{
				fprintf(stderr, "select timeout\n");
				exit(EXIT_FAILURE);
			}

			if (read_frame())
				break;
			/* EAGAIN - continue select loop. */
		}
	}
}

static void stop_capturing(void)
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type))
		errno_exit("VIDIOC_STREAMOFF");
}

static void start_capturing(void)
{
	unsigned int i;
	enum v4l2_buf_type type;

	for (i = 0; i < n_buffers; ++i)
	{
		struct v4l2_buffer buf;

		CLEAR(buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
			errno_exit("VIDIOC_QBUF");
	}
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
		errno_exit("VIDIOC_STREAMON");
}

static void uninit_device(void)
{
	unsigned int i;

	for (i = 0; i < n_buffers; ++i)
		if (-1 == munmap(buffers[i].start, buffers[i].length))
			errno_exit("munmap");

	free(buffers);
}

static void init_mmap(void)
{
	struct v4l2_requestbuffers req;

	CLEAR(req);

	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req))
	{
		if (EINVAL == errno)
		{
			fprintf(stderr, "%s does not support memory mapping\n", dev_name);
			exit(EXIT_FAILURE);
		}
		else
		{
			errno_exit("VIDIOC_REQBUFS");
		}
	}

	if (req.count < 2)
	{
		fprintf(stderr, "Insufficient buffer memory on %s\n", dev_name);
		exit(EXIT_FAILURE);
	}

	buffers = calloc(req.count, sizeof(*buffers));

	if (!buffers)
	{
		fprintf(stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}

	for (n_buffers = 0; n_buffers < req.count; ++n_buffers)
	{
		struct v4l2_buffer buf;

		CLEAR(buf);

		buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory      = V4L2_MEMORY_MMAP;
		buf.index       = n_buffers;

		if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
			errno_exit("VIDIOC_QUERYBUF");

		buffers[n_buffers].length = buf.length;
		buffers[n_buffers].start = mmap(NULL /* start anywhere */,
		                                buf.length,
		                                PROT_READ | PROT_WRITE /* required */,
		                                MAP_SHARED /* recommended */,
		                                fd, buf.m.offset);

		if (MAP_FAILED == buffers[n_buffers].start)
			errno_exit("mmap");
	}
}

static void init_device(void)
{
	struct v4l2_capability cap;
	struct v4l2_format fmt;
	unsigned int min;

	if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap))
	{
		if (EINVAL == errno)
		{
			fprintf(stderr, "%s is no V4L2 device\n", dev_name);
			exit(EXIT_FAILURE);
		}
		else
		{
			errno_exit("VIDIOC_QUERYCAP");
		}
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
	{
		fprintf(stderr, "%s is no video capture device\n", dev_name);
		exit(EXIT_FAILURE);
	}

	if (!(cap.capabilities & V4L2_CAP_STREAMING))
	{
		fprintf(stderr, "%s does not support streaming i/o\n", dev_name);
		exit(EXIT_FAILURE);
	}

	/* Select video input, video standard and tune here. */

	CLEAR(fmt);

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	/* Preserve original settings as set by v4l2-ctl for example */
	if (-1 == xioctl(fd, VIDIOC_G_FMT, &fmt))
		errno_exit("VIDIOC_G_FMT");
	if (force_format)
	{
		fmt.fmt.pix.width  = v4l2_width;
		fmt.fmt.pix.height = v4l2_height;
		fmt.fmt.pix.field  = V4L2_FIELD_ANY;
		/* Note VIDIOC_S_FMT may change width and height. */
	}

	if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV)
	{
		fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
		if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
			errno_exit("VIDIOC_S_FMT");
		if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV)
		{
			fprintf(stderr, "Could not set format to V4L2_PIX_FMT_YUYV\n");
			exit(EXIT_FAILURE);
		}
	}

	v4l2_width = fmt.fmt.pix.width;
	v4l2_height = fmt.fmt.pix.height;
	fprintf(stderr, "v4l2 width: %d\n", v4l2_width);
	fprintf(stderr, "v4l2 height: %d\n", v4l2_height);

	/* Buggy driver paranoia. */
	min = fmt.fmt.pix.width * 2;
	if (fmt.fmt.pix.bytesperline < min)
		fmt.fmt.pix.bytesperline = min;
	min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
	if (fmt.fmt.pix.sizeimage < min)
		fmt.fmt.pix.sizeimage = min;

	init_mmap();
}

static void close_device(void)
{
	if (-1 == close(fd))
		errno_exit("close");

	fd = -1;
}

static void open_device(void)
{
	struct stat st;

	if (-1 == stat(dev_name, &st))
	{
		fprintf(stderr, "Cannot identify '%s': %d, %s\n", dev_name, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (!S_ISCHR(st.st_mode))
	{
		fprintf(stderr, "%s is no device\n", dev_name);
		exit(EXIT_FAILURE);
	}

	fd = open(dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

	if (-1 == fd)
	{
		fprintf(stderr, "Cannot open '%s': %d, %s\n", dev_name, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char **argv)
{
	if (!aa_parseoptions(NULL, NULL, &argc, argv))
	{
		fprintf(stderr, "%s", aa_help);
		exit(EXIT_FAILURE);
	}

	if (argc < 2 || argc > 3)
	{
		fprintf(stderr, "usage: %s [aalib options] [/dev/video0] <widthxheight>\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	dev_name = argv[1];

	if (argc == 3)
	{
		if (sscanf(argv[2], "%dx%d", &v4l2_width, &v4l2_height) != 2)
		{
			fprintf(stderr, "could not parse dimension '%s'\n", argv[2]);
			exit(EXIT_FAILURE);
		}
		force_format = 1;
	}

	aa_save.name = NULL;
	aa_save.format = &aa_ansi_format;
	aa_save.file = stdout;

	ctx = aa_init(&save_d, &aa_defparams, &aa_save);
	if (ctx == NULL)
	{
		fprintf(stderr, "aa_init failed\n");
		exit(EXIT_FAILURE);
	}
	params = aa_getrenderparams();

	aa_width = aa_imgwidth(ctx);
	aa_height = aa_imgheight(ctx);
	fprintf(stderr, "aa width: %d\n", aa_width);
	fprintf(stderr, "aa height: %d\n", aa_height);

	xstep = v4l2_width / (float) aa_width;
	ystep = v4l2_height / (float) aa_height;
	fprintf(stderr, "xstep: %g\n", xstep);
	fprintf(stderr, "ystep: %g\n", ystep);

	open_device();
	init_device();
	start_capturing();
	mainloop();
	stop_capturing();
	uninit_device();
	close_device();
	fprintf(stderr, "\n");

	aa_close(ctx);

	return 0;
}
