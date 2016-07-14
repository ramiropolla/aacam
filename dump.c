#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "fname.h"

static void flush(const char *fname, uint8_t *buf, size_t count)
{
	FILE *fp = fopen(fname, "r+");
	fseek(fp, 0, SEEK_SET);
	fwrite(buf, count, 1, fp);
	fclose(fp);
}

int main()
{
	uint8_t *buf = NULL;
	size_t bufsize = 0;
	size_t count = 0;

	fclose(fopen(FNAME, "w"));

	while ( 42 )
	{
		int c = getchar();
		if ( c == EOF )
			break;
		count++;
		if ( bufsize < count )
		{
			buf = realloc(buf, count);
			bufsize = count;
		}
		buf[count - 1] = c;
		if ( c == '\f' )
		{
			flush(FNAME, buf, count);
			count = 0;
			continue;
		}
	}
	flush(FNAME, buf, count);

	return 0;
}
