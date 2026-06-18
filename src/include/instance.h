#ifndef AVAR_INSTANCE_H
#define AVAR_INSTANCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AVAR_INSTANCE_ID_MAX 63U

bool avar_instance_configured(void);

bool avar_instance_id(char *out, size_t out_size);

const char *avar_app_name(void);

bool avar_path_with_instance(char *out, size_t out_size, const char *path);

bool avar_named_resource_with_instance(char *out, size_t out_size, const char *name);

uint16_t avar_instance_port_offset(void);

#endif
