#ifndef _PLATFORM_UTILS_H
#define _PLATFORM_UTILS_H

#include "LPC17xx.h"

#define htonl(l) __REV(l)
#define ntohl(l) __REV(l)
#define htons(l) __REV16(l)
#define ntohs(l) __REV16(l)

#endif /* _PLATFORM_UTILS_H */
