#ifndef __mpg123_engine_h__
#define __mpg123_engine_h__

struct id3tag {
	char tag[3];
	char title[30];
	char artist[30];
	char album[30];
	char year[4];
	char comment[30];
	unsigned char genre;
};

#endif
