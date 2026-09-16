// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#ifndef RDPLIB_RANDOM_H
#define RDPLIB_RANDOM_H

#ifdef __cplusplus
extern "C"
{
#endif

// rdplib deviation: the default build obtains connection IDs without changing the process global C random stream.
int rdplib_random_next(void);

#ifdef __cplusplus
}
#endif

#endif // RDPLIB_RANDOM_H
