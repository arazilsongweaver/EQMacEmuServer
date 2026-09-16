// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#if defined(_MSC_VER) && defined(RDP_DEAD_CODE)
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif

#include "trace.h"

#ifdef RDP_DEAD_CODE

#include <stdio.h>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netdb.h>
#endif

// unused, retained for historical interest
uint32_t format_trace(char *buffer, trace_probe_t *probes, uint32_t max_probe, uint32_t name_lookup)
{
    uint32_t max_ttl;
    uint32_t j;
    uint32_t i;
    char *position;

    position = buffer;
    max_ttl = 30;
    for (j = 0; j < max_probe; ++j)
    {
        if (probes[j].icmp_type == 3 && probes[j].icmp_code == 3 && probes[j].ttl < max_ttl)
        {
            max_ttl = probes[j].ttl;
        }
    }

    for (i = 1; i <= max_ttl; ++i)
    {
        struct in_addr last_addr;

        last_addr.s_addr = 0;
        position += sprintf(position, " %2u ", i);

        for (j = 0; j < max_probe; ++j)
        {
            if (probes[j].ttl == i)
            {
                if (probes[j].icmp_from.s_addr)
                {
                    char *code;

                    code = NULL;
                    if (probes[j].icmp_from.s_addr != last_addr.s_addr)
                    {
                        struct hostent *he;

                        he = NULL;
                        if (last_addr.s_addr)
                        {
                            position += sprintf(position, "\n    ");
                        }

                        if (name_lookup)
                        {
                            he = gethostbyaddr((const char *)&probes[j].icmp_from, sizeof(probes[j].icmp_from), AF_INET);
                        }

                        if (he && he->h_name)
                        {
                            position += sprintf(position, "%36s ", he->h_name);
                        }
                        else
                        {
                            position += sprintf(position, "%36s ", inet_ntoa(probes[j].icmp_from));
                        }
                    }

                    if (probes[j].icmp_type == 3)
                    {
                        switch (probes[j].icmp_code)
                        {
                        case 0:
                            code = "N";
                            break;
                        case 1:
                            code = "H";
                            break;
                        case 3:
                            code = "";
                            break;
                        case 6:
                            code = "NU";
                            break;
                        case 7:
                            code = "HU";
                            break;
                        }
                    }
                    else if (probes[j].icmp_type == 11 && probes[j].icmp_code == 0)
                    {
                        code = "";
                    }

                    if (code)
                    {
                        position += sprintf(position, "%4u%s ", probes[j].reply_time, code);
                    }
                    else
                    {
                        position += sprintf(position, "%u ICMP[%u][%u] ", probes[j].reply_time, probes[j].icmp_type, probes[j].icmp_code);
                    }

                    last_addr = probes[j].icmp_from;
                }
                else
                {
                    position += sprintf(position, "   * ");
                }
            }
        }

        position += sprintf(position, "\n");
    }

    return (uint32_t)(position - buffer);
}

#endif /* RDP_DEAD_CODE */
