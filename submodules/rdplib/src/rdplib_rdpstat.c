// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#include "rdpstat.h"

#if defined(_MSC_VER)
#define RDPLIB_THREAD_LOCAL __declspec(thread)
#else
#define RDPLIB_THREAD_LOCAL _Thread_local
#endif

static RDPLIB_THREAD_LOCAL rdp_stat rdplib_discarded_statistics_storage;
static int rdplib_discard_statistics;

void rdplib_discard_global_statistics(int discard)
{
    rdplib_discard_statistics = discard != 0;
}

rdp_stat *rdplib_discarded_statistics(void)
{
    return rdplib_discard_statistics ? &rdplib_discarded_statistics_storage : g_rdp_stat;
}
