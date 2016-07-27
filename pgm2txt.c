#include <stdio.h>
#include <stdlib.h>

#include <aalib.h>
#include <inttypes.h>

#include "pgm.h"

static int              img_width;
static int              img_height;

static aa_context      *ctx;
static aa_renderparams *params;
static float            xstep;
static float            ystep;
static int              aa_width;
static int              aa_height;

static int img_index(int x, int y)
{
	int idx = (float) y * ystep;
	idx *= img_width;
	idx += (float) x * xstep;
	return idx;
}

static void process_image(const uint8_t *p)
{
	for (int y = 0; y < aa_height; y++)
		for (int x = 0; x < aa_width; x++)
			aa_putpixel(ctx, x, y, p[img_index(x, y)]);
	aa_render(ctx, params, 0, 0, aa_scrwidth(ctx), aa_scrheight(ctx));
	aa_flush(ctx);
}

int main(int argc, char **argv)
{
	uint8_t *img;

	if (!aa_parseoptions(NULL, NULL, &argc, argv))
	{
		fprintf(stderr, "%s", aa_help);
		exit(EXIT_FAILURE);
	}

	if (argc < 2)
	{
		fprintf(stderr, "usage: %s [aalib options] [pgm file]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	img = pgm_read(argv[1], &img_width, &img_height);
	if (img == NULL)
	{
		fprintf(stderr, "Could not open image '%s'\n", argv[1]);
		exit(EXIT_FAILURE);
	}

	ctx = aa_autoinit(&aa_defparams);
	if (ctx == NULL)
	{
		fprintf(stderr, "aa_autoinit failed\n");
		exit(EXIT_FAILURE);
	}
	params = aa_getrenderparams();

	aa_width = aa_imgwidth(ctx);
	aa_height = aa_imgheight(ctx);
	fprintf(stderr, "aa width: %d\n", aa_width);
	fprintf(stderr, "aa height: %d\n", aa_height);

	xstep = img_width / (float) aa_width;
	ystep = img_height / (float) aa_height;
	fprintf(stderr, "xstep: %g\n", xstep);
	fprintf(stderr, "ystep: %g\n", ystep);

	process_image(img);

	aa_close(ctx);

	free(img);

	return 0;
}
