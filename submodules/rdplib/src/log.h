// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#ifndef RDP_LOG_H
#define RDP_LOG_H

// discard logging is restored only by source faithful builds.
#ifdef RDPLIB_SOURCE_FAITHFUL

#include <stdio.h>

#ifdef __cplusplus
extern "C"
{
#endif

void time_format(char *time_text);
void ftimeprint(FILE *file);
void discard_log_append(char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* RDPLIB_SOURCE_FAITHFUL */

#endif /* RDP_LOG_H */
