// Copyright (c) 2026 solar
// SPDX-License-Identifier: MIT

#if defined(_MSC_VER) && (defined(RDPLIB_DEBUG) || defined(RDPLIB_SOURCE_FAITHFUL) || defined(RDP_DEAD_CODE))
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "ustrerror.h"

// Recovered diagnostic code. sys_strerror is unused in the May archive and is
// retained for historical interest; net_strerror served diagnostic callers.
#if defined(RDPLIB_DEBUG) || defined(RDPLIB_SOURCE_FAITHFUL) || defined(RDP_DEAD_CODE)

#include <stdio.h>

static char net_error_string[64];
#ifdef RDP_DEAD_CODE
static char sys_error_string[64];

char *sys_strerror(uint32_t error_number)
{
    char *result;

    result = sys_error_string;
    sprintf(sys_error_string, "%u", error_number);
    return result;
}
#endif

char *net_strerror(uint32_t error_number)
{
    char *result;

    result = net_error_string;
    switch (error_number)
    {
    case 10004:
        result = "WSAEINTR";
        break;
    case 10009:
        result = "WSAEBADF";
        break;
    case 10013:
        result = "WSAEACCES";
        break;
    case 10014:
        result = "WSAEFAULT";
        break;
    case 10022:
        result = "WSAEINVAL";
        break;
    case 10024:
        result = "WSAEMFILE";
        break;
    case 10035:
        result = "WSAEWOULDBLOCK";
        break;
    case 10036:
        result = "WSAEINPROGRESS";
        break;
    case 10037:
        result = "WSAEALREADY";
        break;
    case 10039:
        result = "WSAEDESTADDRREQ";
        break;
    case 10040:
        result = "WSAEMSGSIZE";
        break;
    case 10041:
        result = "WSAEPROTOTYPE";
        break;
    case 10042:
        result = "WSAENOPROTOOPT";
        break;
    case 10043:
        result = "WSAEPROTONOSUPPORT";
        break;
    case 10044:
        result = "WSAESOCKTNOSUPPORT";
        break;
    case 10045:
        result = "WSAEOPNOTSUPP";
        break;
    case 10046:
        result = "WSAEPFNOSUPPORT";
        break;
    case 10047:
        result = "WSAEAFNOSUPPORT";
        break;
    case 10048:
        result = "WSAEADDRINUSE";
        break;
    case 10049:
        result = "WSAEADDRNOTAVAIL";
        break;
    case 10050:
        result = "WSAENETDOWN";
        break;
    case 10051:
        result = "WSAENETUNREACH";
        break;
    case 10052:
        result = "WSAENETRESET";
        break;
    case 10053:
        result = "WSAECONNABORTED";
        break;
    case 10054:
        result = "WSAECONNRESET";
        break;
    case 10056:
        result = "WSAEISCONN";
        break;
    case 10057:
        result = "WSAENOTCONN";
        break;
    case 10058:
        result = "WSAESHUTDOWN";
        break;
    case 10059:
        result = "WSAETOOMANYREFS";
        break;
    case 10060:
        result = "WSAETIMEDOUT";
        break;
    case 10061:
        result = "WSAECONNREFUSED";
        break;
    case 10062:
        result = "WSAELOOP";
        break;
    case 10063:
        result = "WSAENAMETOOLONG";
        break;
    case 10064:
        result = "WSAEHOSTDOWN";
        break;
    case 10065:
        result = "WSAEHOSTUNREACH";
        break;
    case 10066:
        result = "WSAENOTEMPTY";
        break;
    case 10067:
        result = "WSAEPROCLIM";
        break;
    case 10068:
        result = "WSAEUSERS";
        break;
    case 10069:
        result = "WSAEDQUOT";
        break;
    case 10070:
        result = "WSAESTALE";
        break;
    case 10071:
        result = "WSAEREMOTE";
        break;
    case 10091:
        result = "WSASYSNOTREADY";
        break;
    case 10092:
        result = "WSAVERNOTSUPPORTED";
        break;
    case 10093:
        result = "WSANOTINITIALISED";
        break;
    case 10101:
        result = "WSAEDISCON";
        break;
#if defined(RDPLIB_SOURCE_FAITHFUL) && !defined(_WIN32)
    // Mac clients extend the canonical table with these entries.
    case 10102:
        result = "WSAENOMORE";
        break;
    case 10103:
        result = "WSAECANCELLED";
        break;
    case 10104:
        result = "WSAEINVALIDPROCTABLE";
        break;
    case 10105:
        result = "WSAEINVALIDPROVIDER";
        break;
    case 10106:
        result = "WSAEPROVIDERFAILEDINIT";
        break;
    case 10107:
        result = "WSASYSCALLFAILURE";
        break;
    case 10108:
        result = "WSASERVICE_NOT_FOUND";
        break;
    case 10109:
        result = "WSATYPE_NOT_FOUND";
        break;
    case 10110:
        result = "WSA_E_NO_MORE";
        break;
    case 10111:
        result = "WSA_E_CANCELLED";
        break;
    case 10112:
        result = "WSAEREFUSED";
        break;
#endif
    case 11001:
        result = "WSAHOST_NOT_FOUND";
        break;
    case 11002:
        result = "WSATRY_AGAIN";
        break;
    case 11003:
        result = "WSANO_RECOVERY";
        break;
    case 11004:
        result = "WSANO_DATA";
        break;
    default:
#if defined(RDPLIB_SOURCE_FAITHFUL) && !defined(_WIN32) && !defined(__powerpc__) && !defined(__ppc__) && !defined(_M_PPC)
        // PowerPC Mac uses %u; Intel Mac uses %lu.
        sprintf(net_error_string, "%lu", (unsigned long)error_number);
#else
        sprintf(net_error_string, "%u", error_number);
#endif
        break;
    }
    return result;
}

#ifdef RDP_DEAD_CODE
char *net_strerror_mac(uint32_t error_number)
{
    switch (error_number)
    {
    case 10102:
        return "WSAENOMORE";
    case 10103:
        return "WSAECANCELLED";
    case 10104:
        return "WSAEINVALIDPROCTABLE";
    case 10105:
        return "WSAEINVALIDPROVIDER";
    case 10106:
        return "WSAEPROVIDERFAILEDINIT";
    case 10107:
        return "WSASYSCALLFAILURE";
    case 10108:
        return "WSASERVICE_NOT_FOUND";
    case 10109:
        return "WSATYPE_NOT_FOUND";
    case 10110:
        return "WSA_E_NO_MORE";
    case 10111:
        return "WSA_E_CANCELLED";
    case 10112:
        return "WSAEREFUSED";
    default:
        return net_strerror(error_number);
    }
}
#endif

#else

typedef int rdplib_ustrerror_disabled_translation_unit;

#endif /* RDPLIB_DEBUG || RDPLIB_SOURCE_FAITHFUL || RDP_DEAD_CODE */
