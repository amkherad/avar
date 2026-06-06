#ifndef AVAR_CONFIG_H
#define AVAR_CONFIG_H
#include "utils.h"

string get_config(stringa key);

string get_config_or_default(stringa key, string defaultValue);

void set_config(stringa key, string value);

void save_config(string path);

#endif
