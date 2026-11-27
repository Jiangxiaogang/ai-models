#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "config.h"

#if defined(_MSC_VER)
#define strcasecmp _stricmp
#endif

static void trim(char *s) {
	int b = 0, e, i, len;
	while (s[b] == ' ' || s[b] == '\t') b++;
	len = (int)strlen(s);
	e = len - 1;
	while (e >= b && (s[e] == ' ' || s[e] == '\t' || s[e] == '\r' || s[e] == '\n')) e--;
	for (i = 0; i <= e - b; i++) s[i] = s[b + i];
	s[i] = 0;
	if (s[0] == '"') {
		len = (int)strlen(s);
		memmove(s, s + 1, len);
		if (s[len - 2] == '"') s[len - 2] = 0;
	}
}

static char *skip_ns(char *s) {
	while (*s && *s != ' ' && *s != '\t' && *s != '=') s++;
	return s;
}

int config_load(config_t *cfg, const char *path) {
	FILE *f;
	char line[512], *k, *v;
	cfg->port = 8080;
	strcpy(cfg->root, ".");
	f = fopen(path, "r");
	if (!f) return 1;
	while (fgets(line, sizeof(line), f)) {
		k = line;
		while (*k == ' ' || *k == '\t') k++;
		if (*k == '#' || *k == '\r' || *k == '\n' || *k == 0) continue;
		v = skip_ns(k);
		if (*v) { *v = 0; v++; }
		while (*v == ' ' || *v == '\t' || *v == '=') v++;
		trim(k);
		trim(v);
		if (strcmp(k, "port") == 0) cfg->port = (unsigned short)atoi(v);
		else if (strcmp(k, "root") == 0) strcpy(cfg->root, v);
	}
	fclose(f);
	return 0;
}