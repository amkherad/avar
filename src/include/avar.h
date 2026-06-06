#ifndef AVAR_H
#define AVAR_H

#include "logger.h"
#include "config.h"
#include "cli.h"
#include "stdlib.h"

#define STR(x) #x
#define TOSTR(x) STR(x)

#define APP_NAME "Avar"
#define APP_ID "avar" // same as APP_NAME but in lower-case normalized form
#define VERSION 0.0.1
#define VERSION_STR TOSTR(VERSION)

void fatal(stringa reasonOrNull);

#endif
