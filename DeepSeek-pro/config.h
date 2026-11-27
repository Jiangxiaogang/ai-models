#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
	unsigned short port;
	char root[260];
} config_t;

int config_load(config_t *cfg, const char *path);

#endif