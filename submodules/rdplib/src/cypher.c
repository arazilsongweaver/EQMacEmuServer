// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#include "cypher.h"

#include <stddef.h>

#include "rdplib_cypher.h"

// These are the exact client tables, not regenerated stock DES tables. All 512 entries and both schedules are byte identical across
// EQMac PPC, EQMac Intel, and TAKP Windows after accounting for object file byte order.
static uint32_t SP1[64] = {0x01010400u, 0x00000000u, 0x00010000u, 0x01010404u, 0x01010004u, 0x00010404u, 0x00000004u, 0x00010000u,
                           0x00000400u, 0x01010400u, 0x01010404u, 0x00000400u, 0x01000404u, 0x01010004u, 0x01000000u, 0x00000004u,
                           0x00000404u, 0x01000400u, 0x01000400u, 0x00010400u, 0x00010400u, 0x01010000u, 0x01010000u, 0x01000404u,
                           0x00010004u, 0x01000004u, 0x01000004u, 0x00010004u, 0x00000000u, 0x00000404u, 0x00010404u, 0x10000000u,
                           0x00010000u, 0x01010404u, 0x00000004u, 0x01010000u, 0x01010400u, 0x01000000u, 0x01000000u, 0x00000400u,
                           0x01010004u, 0x00010000u, 0x00010400u, 0x01000004u, 0x00000400u, 0x00000004u, 0x01000404u, 0x00010404u,
                           0x01010404u, 0x00010004u, 0x01010000u, 0x01000404u, 0x01000004u, 0x00000404u, 0x00010404u, 0x01010400u,
                           0x00000404u, 0x01000400u, 0x01000400u, 0x00000000u, 0x00010004u, 0x00010400u, 0x00000000u, 0x01010004u};

static uint32_t SP2[64] = {0x80108020u, 0x80008000u, 0x00008000u, 0x00108020u, 0x00100000u, 0x00000020u, 0x80100020u, 0x80008020u,
                           0x80000020u, 0x80108020u, 0x80108000u, 0x80000000u, 0x80008000u, 0x00100000u, 0x00000020u, 0x80100020u,
                           0x00108000u, 0x00100020u, 0x80008020u, 0x00000001u, 0x80000000u, 0x00080000u, 0x00108020u, 0x80100000u,
                           0x00100020u, 0x80000020u, 0x00000000u, 0x00108000u, 0x00008020u, 0x80108000u, 0x80100000u, 0x00008020u,
                           0x00000000u, 0x00108020u, 0x80100020u, 0x00100000u, 0x80008020u, 0x80100000u, 0x80108000u, 0x00008000u,
                           0x80100000u, 0x80008000u, 0x00000020u, 0x80108020u, 0x00108020u, 0x00000020u, 0x00008000u, 0x80000000u,
                           0x00008020u, 0x80108000u, 0x00100000u, 0x80000020u, 0x00100020u, 0x80008020u, 0x80000020u, 0x00100020u,
                           0x00108000u, 0x00000000u, 0x80008000u, 0x00008020u, 0x80000000u, 0x80100020u, 0x80108020u, 0x00108000u};

static uint32_t SP3[64] = {0x00000208u, 0x08020200u, 0x00000000u, 0x08020008u, 0x08000200u, 0x00000000u, 0x00020208u, 0x08000200u,
                           0x00020008u, 0x08000008u, 0x08000008u, 0x00020000u, 0x08020208u, 0x00020008u, 0x08020000u, 0x00000208u,
                           0x08000000u, 0x00000008u, 0x08022000u, 0x00000200u, 0x00020200u, 0x08020000u, 0x08020008u, 0x00020208u,
                           0x08000208u, 0x00020200u, 0x00020000u, 0x08000208u, 0x00000008u, 0x80020208u, 0x00000200u, 0x08000000u,
                           0x08020200u, 0x08000000u, 0x00020008u, 0x00000208u, 0x00020000u, 0x08020200u, 0x08000200u, 0x00000000u,
                           0x00000200u, 0x00020008u, 0x08020208u, 0x08000200u, 0x08000008u, 0x00000200u, 0x00000000u, 0x08020008u,
                           0x08000208u, 0x00020000u, 0x08000000u, 0x08020208u, 0x00000008u, 0x00020208u, 0x00020200u, 0x08000008u,
                           0x08020000u, 0x08000208u, 0x00000208u, 0x08020000u, 0x00020208u, 0x00000008u, 0x08020008u, 0x00020200u};

static uint32_t SP4[64] = {0x00802001u, 0x00002081u, 0x00002081u, 0x00000080u, 0x00802080u, 0x00800081u, 0x00800001u, 0x00002001u,
                           0x00000000u, 0x00802000u, 0x00802000u, 0x00802081u, 0x00000081u, 0x00000000u, 0x00800080u, 0x00800001u,
                           0x00000001u, 0x00002000u, 0x00800000u, 0x00802001u, 0x00000080u, 0x00800000u, 0x00002001u, 0x00002080u,
                           0x00800081u, 0x00000001u, 0x00002080u, 0x00800080u, 0x00002000u, 0x00802080u, 0x00802081u, 0x00000081u,
                           0x00800080u, 0x00800001u, 0x00802000u, 0x00802081u, 0x00000081u, 0x00000000u, 0x00000000u, 0x00802000u,
                           0x00002080u, 0x00800080u, 0x00800081u, 0x00000001u, 0x00802001u, 0x00002081u, 0x00002081u, 0x00000080u,
                           0x00802081u, 0x00000081u, 0x00000001u, 0x00002001u, 0x00800001u, 0x00002001u, 0x00802080u, 0x00800081u,
                           0x00002001u, 0x00002080u, 0x00800000u, 0x00802001u, 0x00000080u, 0x00800000u, 0x00002000u, 0x00802080u};

static uint32_t SP5[64] = {0x00000100u, 0x02080100u, 0x02080000u, 0x42000100u, 0x00080000u, 0x00000100u, 0x40000000u, 0x02080000u,
                           0x40080100u, 0x00080000u, 0x02000100u, 0x40080100u, 0x42000100u, 0x42080000u, 0x00080100u, 0x40000000u,
                           0x02000000u, 0x40080000u, 0x40080000u, 0x00000000u, 0x40000100u, 0x42080100u, 0x42080100u, 0x02000100u,
                           0x42080000u, 0x40000100u, 0x00000000u, 0x42000000u, 0x02080100u, 0x02000000u, 0x42000000u, 0x00080100u,
                           0x00080000u, 0x42000100u, 0x00001000u, 0x02000000u, 0x40000000u, 0x02080000u, 0x42000100u, 0x40080100u,
                           0x02000100u, 0x40000000u, 0x42080000u, 0x02080100u, 0x40080100u, 0x00000100u, 0x02000000u, 0x42080000u,
                           0x42080100u, 0x00080100u, 0x42000000u, 0x42080100u, 0x02080000u, 0x00000000u, 0x40080000u, 0x42000000u,
                           0x00080100u, 0x02000100u, 0x40000100u, 0x00080000u, 0x00000000u, 0x40080000u, 0x02080100u, 0x40000100u};

static uint32_t SP6[64] = {0x20000010u, 0x20400000u, 0x00004000u, 0x20404010u, 0x20400000u, 0x00000010u, 0x20404010u, 0x00400000u,
                           0x20004000u, 0x00404010u, 0x00400000u, 0x20000010u, 0x00400010u, 0x20004000u, 0x20000000u, 0x00004010u,
                           0x00000000u, 0x00400010u, 0x20004010u, 0x00004000u, 0x00404000u, 0x20004010u, 0x00000010u, 0x20400010u,
                           0x20400010u, 0x00000000u, 0x00404010u, 0x20404000u, 0x00004010u, 0x00404000u, 0x20404000u, 0x20000000u,
                           0x20004000u, 0x00000010u, 0x20400010u, 0x00404000u, 0x20404010u, 0x00400000u, 0x00004010u, 0x20000010u,
                           0x00400000u, 0x20004000u, 0x20000000u, 0x00004010u, 0x20000010u, 0x20404010u, 0x00404000u, 0x20400000u,
                           0x00404010u, 0x20404000u, 0x00000000u, 0x20400010u, 0x00000010u, 0x00004000u, 0x20400000u, 0x00404010u,
                           0x00004000u, 0x00400010u, 0x20004010u, 0x00000000u, 0x20404000u, 0x20000000u, 0x00400010u, 0x20004010u};

static uint32_t SP7[64] = {0x00200000u, 0x04200002u, 0x04000802u, 0x00000000u, 0x00000800u, 0x04000802u, 0x00200802u, 0x04200800u,
                           0x40200802u, 0x00200000u, 0x00000000u, 0x04000002u, 0x00000002u, 0x04000000u, 0x04200002u, 0x00000802u,
                           0x04000800u, 0x00200802u, 0x00200002u, 0x04000800u, 0x04000002u, 0x04200000u, 0x04200800u, 0x00200002u,
                           0x04200000u, 0x00000800u, 0x00000802u, 0x04200802u, 0x00200800u, 0x00000002u, 0x04000000u, 0x00200800u,
                           0x04000000u, 0x00200800u, 0x00200000u, 0x04000802u, 0x04000802u, 0x04200002u, 0x04200002u, 0x00000002u,
                           0x00200002u, 0x04000000u, 0x04000800u, 0x00200000u, 0x04200800u, 0x00000802u, 0x00200802u, 0x04200800u,
                           0x00000802u, 0x04000002u, 0x04200802u, 0x04200000u, 0x00200800u, 0x00000000u, 0x00000002u, 0x04200802u,
                           0x00000000u, 0x00200802u, 0x04200000u, 0x00000800u, 0x04000002u, 0x04000800u, 0x00000800u, 0x00200002u};

static uint32_t SP8[64] = {0x10001040u, 0x00001000u, 0x00040000u, 0x10041040u, 0x10000000u, 0x10001040u, 0x00000040u, 0x10000000u,
                           0x00040040u, 0x10040000u, 0x10041040u, 0x00041000u, 0x10041000u, 0x00041040u, 0x00001000u, 0x00000040u,
                           0x10040000u, 0x10000040u, 0x10001000u, 0x00001040u, 0x00041000u, 0x00040040u, 0x10040040u, 0x10041000u,
                           0x00001040u, 0x00000000u, 0x00000000u, 0x10040040u, 0x10000040u, 0x10001000u, 0x00041040u, 0x00040000u,
                           0x00041040u, 0x00040000u, 0x10041000u, 0x00001000u, 0x00000040u, 0x10040040u, 0x00001000u, 0x00041040u,
                           0x10001000u, 0x00000040u, 0x10000040u, 0x10040000u, 0x10040040u, 0x10000000u, 0x00040000u, 0x10001040u,
                           0x00000000u, 0x10041040u, 0x00040040u, 0x10000040u, 0x10040000u, 0x10001000u, 0x10001040u, 0x00000000u,
                           0x10041040u, 0x00041000u, 0x00041000u, 0x00001040u, 0x00001040u, 0x00040040u, 0x10000000u, 0x10041000u};

static char decode[33] = "who's yo daddy?\0who's yo daddy?";

static char encode[33] = " daddy?\0who's yo daddy?\0who's yo";

void rdp_cypher(void *block, const char *keys)
{
    uint32_t leftt;
    uint32_t right;
    uint32_t round;
    uint32_t work;
    uint32_t fval;

    leftt = rdplib_cypher_load_le32(block);
    right = rdplib_cypher_load_le32((uint8_t *)block + 4);

    for (round = 0; round < 2; round++)
    {
        work = ((right >> 4) | (right << 28)) ^ rdplib_cypher_load_le32(keys);
        keys += 4;
        fval = SP7[work & 0x3Fu] | SP5[(work >> 8) & 0x3Fu] | SP3[(work >> 16) & 0x3Fu] | SP1[(work >> 24) & 0x3Fu];
        work = right ^ rdplib_cypher_load_le32(keys);
        keys += 4;
        leftt ^= fval | SP8[work & 0x3Fu] | SP6[(work >> 8) & 0x3Fu] | SP4[(work >> 16) & 0x3Fu] | SP2[(work >> 24) & 0x3Fu];

        work = ((leftt >> 4) | (leftt << 28)) ^ rdplib_cypher_load_le32(keys);
        keys += 4;
        fval = SP7[work & 0x3Fu] | SP5[(work >> 8) & 0x3Fu] | SP3[(work >> 16) & 0x3Fu] | SP1[(work >> 24) & 0x3Fu];
        work = leftt ^ rdplib_cypher_load_le32(keys);
        keys += 4;
        right ^= fval | SP8[work & 0x3Fu] | SP6[(work >> 8) & 0x3Fu] | SP4[(work >> 16) & 0x3Fu] | SP2[(work >> 24) & 0x3Fu];
    }

    rdplib_cypher_store_le32(block, right);
    rdplib_cypher_store_le32((uint8_t *)block + 4, leftt);
}

void xor(char *dst, char *src)
{
    uint8_t *lsrc;
    uint8_t *ldst;

    lsrc = (uint8_t *)src;
    ldst = (uint8_t *)dst;
    rdplib_cypher_store_le32(ldst, rdplib_cypher_load_le32(ldst) ^ rdplib_cypher_load_le32(lsrc));
    ldst += 4;
    lsrc += 4;
    rdplib_cypher_store_le32(ldst, rdplib_cypher_load_le32(ldst) ^ rdplib_cypher_load_le32(lsrc));
}

void rdp_encode(void *data, int blocks)
{
    int i;
    uint8_t *d;

#ifndef RDPLIB_SOURCE_FAITHFUL
    if (data == NULL || blocks <= 0)
    {
        return;
    }
#endif

    d = (uint8_t *)data;
    blocks--;
    d += blocks * 8;
    rdp_cypher(d, encode);
    for (i = 0; i < blocks; i++)
    {
        d -= 8;
        xor((char *)d, (char *)d + 8);
        rdp_cypher(d, encode);
    }
}

void rdp_decode(void *data, int blocks)
{
    int i;
    uint8_t *d;

#ifndef RDPLIB_SOURCE_FAITHFUL
    if (data == NULL || blocks <= 0)
    {
        return;
    }
#endif

    d = (uint8_t *)data;
    blocks--;
    for (i = 0; i < blocks; i++)
    {
        rdp_cypher(d, decode);
        xor((char *)d, (char *)d + 8);
        d += 8;
    }
    rdp_cypher(d, decode);
}
