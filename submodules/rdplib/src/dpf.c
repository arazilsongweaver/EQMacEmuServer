// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

// this module intentionally omits set_dpf_print_mask and the body of dpf.
// this stub is just so we can show the recovered call sites.

#include "dpf.h"

#ifdef RDPLIB_DEBUG

void dpf(uint32_t filter, const char *fmt, ...)
{
    // deliberate empty implementation
    (void)filter;
    (void)fmt;
}

uint32_t data_format(char *dst, const uint8_t *data, uint32_t size)
{
    static char s_hex_chars[17] = "0123456789ABCDEF";
    char *text;
    uint32_t i;
    uint32_t j;
    uint8_t d;

    text = dst;
    for (;;)
    {
        for (i = 0; i < 8u; ++i)
        {
            for (j = 0; j < 4u; ++j)
            {
                if (size == 0)
                {
                    *text++ = '\0';
                    return (uint32_t)(text - dst - 1);
                }
                d = *data++;
                *text++ = s_hex_chars[d >> 4];
                *text++ = s_hex_chars[d & 0x0Fu];
                --size;
            }
            *text++ = ' ';
        }
        *text++ = '\n';
    }
}

#else

typedef int rdplib_dpf_disabled_translation_unit;

#endif /* RDPLIB_DEBUG */
