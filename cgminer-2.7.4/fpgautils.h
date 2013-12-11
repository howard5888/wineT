/*
 * Copyright 2012 Luke Dashjr
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.  See COPYING for more details.
 */

#ifndef FPGAUTILS_H
#define FPGAUTILS_H

#include <stdbool.h>
#include <stdio.h>

typedef bool(*detectone_func_t)(const char*);
typedef int(*autoscan_func_t)();

extern int _serial_detect(const char*dname, detectone_func_t, autoscan_func_t, bool force_autoscan);
#define serial_detect_fauto(dname, detectone, autoscan)  \
	_serial_detect(dname, detectone, autoscan, true)
#define serial_detect_auto(dname, detectone, autoscan)  \
	_serial_detect(dname, detectone, autoscan, false)
#define serial_detect(dname, detectone)  \
	_serial_detect(dname, detectone,     NULL, false)
extern int serial_autodetect_devserial(detectone_func_t, const char*prodname);
extern int serial_autodetect_udev     (detectone_func_t, const char*prodname);

extern int serial_open(const char*devpath, unsigned long baud, signed short timeout, bool purge);
extern ssize_t _serial_read(int fd, char *buf, size_t buflen, char*eol);
#define serial_read(fd, buf, count)  \
	_serial_read(fd, (char*)(buf), count, NULL)
#define serial_read_line(fd, buf, bufsiz, eol)  \
	_serial_read(fd, buf, count, &eol)
#define serial_close(fd)  close(fd)

extern FILE*open_bitstream(const char*dname, const char*filename);

#endif
