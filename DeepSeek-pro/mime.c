#include <string.h>
#include "mime.h"

#if defined(_MSC_VER)
#define strcasecmp _stricmp
#endif

static const struct {
	const char *ext;
	const char *type;
} tbl[] = {
	{"html","text/html; charset=utf-8"}, {"htm","text/html; charset=utf-8"},
	{"css","text/css"}, {"js","application/javascript"},
	{"json","application/json"}, {"xml","text/xml"},
	{"txt","text/plain; charset=utf-8"}, {"csv","text/csv"},
	{"png","image/png"}, {"jpg","image/jpeg"}, {"jpeg","image/jpeg"},
	{"gif","image/gif"}, {"svg","image/svg+xml"}, {"ico","image/x-icon"},
	{"bmp","image/bmp"}, {"webp","image/webp"},
	{"mp3","audio/mpeg"}, {"wav","audio/wav"}, {"ogg","audio/ogg"},
	{"mp4","video/mp4"}, {"webm","video/webm"},
	{"pdf","application/pdf"}, {"zip","application/zip"},
	{"gz","application/gzip"}, {"tar","application/x-tar"},
	{"7z","application/x-7z-compressed"}, {"rar","application/x-rar-compressed"},
	{"doc","application/msword"}, {"docx","application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
	{"xls","application/vnd.ms-excel"}, {"xlsx","application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
	{0,0}
};

const char *mime_type(const char *path) {
	const char *dot = strrchr(path, '.');
	int i;
	if (!dot) return "application/octet-stream";
	dot++;
	for (i = 0; tbl[i].ext; i++)
		if (strcasecmp(tbl[i].ext, dot) == 0)
			return tbl[i].type;
	return "application/octet-stream";
}