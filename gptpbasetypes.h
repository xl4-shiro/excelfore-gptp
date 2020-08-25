/*
 * Excelfore gptp - Implementation of gPTP(IEEE 802.1AS)
 * Copyright (C) 2019 Excelfore Corporation (https://excelfore.com)
 *
 * This file is part of Excelfore-gptp.
 *
 * Excelfore-gptp is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Excelfore-gptp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Excelfore-gptp.  If not, see
 * <https://www.gnu.org/licenses/old-licenses/gpl-2.0.html>.
 */
/**
 * @addtogroup gptp
 * @{
 * @file gptpbasetypes.h
 * @author Shiro Ninomiya <shiro@excelfore.com>
 * @copyright Copyright (C) 2017-2018 Excelfore Corporation
 * @brief File contains base datatypes used, as per IEEE 802.1AS
 * Standard Documentation.
 */

#ifndef __GPTPBASETYPES_H_
#define __GPTPBASETYPES_H_

#include <string.h>

/**
 * @brief Macro which defines Maximum port number limit for gPTP2.
 * @note CONF_MAX_PORT_NUMBER can be configured at a runtime,
 * 	 it must be less than this numumber
 */
#define MAX_PORT_NUMBER_LIMIT 33

/**
 * @brief Macro to define maximum path trace.
 * @note the possible maximum number is 179 but we won't have such deep layers
 * and limit to a smaller number here
 * @code{.c}
 * #define MAX_PATH_TRACE_N 179
 * @endcode
 * @see 10.3.8.23 pathTrace
 */
#define MAX_PATH_TRACE_N 16

/**
 * @brief Macro which defines Clock Identity Length.
 */
#define CLOCK_IDENTITY_LENGTH	  8

typedef uint8_t ClockIdentity[CLOCK_IDENTITY_LENGTH];

/**
 * @brief Primitive data types specifications for IEEE 802.1AS.
 * All non-primitive data types are derived from the primitive types, signed integers are
 * represented as two's complement form.
 * @verbatim See the following Table@endverbatim
 * Data Types   |         Definition
 * ----------   | ----------------------------------------------
 * EnumerationN | N-bit enumerated value
 * UIntegerN    | N-bit unsigned integer
 * Nibble       | 4-bit field not interpreted as a number
 * Octet	| 8-bit field not interpreted as a number
 * OctetN	| N-octet fiels not interpreted as a number
 * Double 	| Double precision (64-bit) floating-point vlaue
 */

typedef uint8_t Nibble;
typedef uint8_t Octet;
typedef uint8_t Octet2[2];
typedef uint8_t Octet3[3];
typedef uint8_t Octet4[4];
typedef uint8_t Enumeration2;
typedef uint8_t Enumeration4;
typedef uint8_t Enumeration8;
typedef uint16_t Enumeration16;
typedef uint32_t Enumeration24;
typedef uint8_t UInteger4;

/**
 * @brief The portIdentity identifies a port of a time-aware system.
 * @note ClockIdentity is a array of unsigned char which is used to identify time-aware system.
 */
typedef struct PortIdentity {
	ClockIdentity clockIdentity;
	uint16_t portNumber;
} PortIdentity;

/**
 * @brief The clock quality represents quality of clock.
 */
typedef struct ClockQuality {
	uint8_t clockClass;
	Enumeration8 clockAccuracy;
	uint16_t offsetScaledLogVariance;
} ClockQuality;

/**
 * @brief 48bit variable, lsb is 32 bits
 */
typedef struct UInteger48 {
	uint32_t lsb;
	uint16_t msb;
} UInteger48;
/**
 * @brief 48bit variable, msb is 32 bits
 */
typedef struct UInteger48m32 {
	uint16_t lsb;
	uint32_t msb;
} UInteger48m32;

/**
 * @brief UInteger112 (802.1AS, 10.3.2 systemIdentity)
 */
typedef struct UInteger112 {
	uint8_t priority1;
	uint8_t clockClass;
	uint8_t clockAccuracy;
	uint16_t offsetScaledLogVariance;
	uint8_t priority2;
	ClockIdentity clockIdentity;
} __attribute__((packed, aligned(1))) UInteger112;

/**
 * @brief UInteger224 (802.1AS, 10.3.4 time-synchronization spanning
 * tree priority vectors )
 */
typedef struct UInteger224 {
        UInteger112 rootSystemIdentity;
        uint16_t stepsRemoved;
        PortIdentity sourcePortIdentity;
        uint16_t portNumber;
} __attribute__((packed, aligned(1))) UInteger224;

/**
 * @brief The ScaledNs type represents signed values of time and time
 * interval in units of 2e-16 ns.
 */
typedef struct ScaledNs {
	uint16_t subns;
	int64_t nsec;
	int16_t nsec_msb;
} ScaledNs;

/**
 * @brief The ScaledNs type represents unsigned values of time and
 * time interval in units of 2^-16 ns.
 * @code
 * //2.5 ns expressed as below
 * 0x0000 0000 0000 0000 0002 8000
 * @endcode
 *
 */
typedef struct UScaledNs {
	uint16_t subns;
	uint64_t nsec;
	uint16_t nsec_msb;
} UScaledNs;

/*
 * @brief The TimeInterval type represents time intervals, in units of 2^-16 ns
 * @verbatim Example to express ns@endverbatim
 * @code
 * //2.5 ns expressed as below
 * 0x0000 0000 0002 8000
 * @endcode
 *
 */
typedef struct TimeInterval {
	int64_t scaledNanoseconds;
} TimeInterval;

/**
 * @brief The Timestamp type represents a positive time with respect to the epoch.
 * @verbatim For example:@endverbatim
 * @code
 * +2.000000001 seconds is represented by seconds = 0x0000 0000 0002 and nanoseconds= 0x0000 0001
 * @endcode
 *
 */
typedef struct Timestamp {
	uint32_t nanoseconds;
	UInteger48 seconds;
} Timestamp;

/**
 * @brief The ExtendTimestamp type represents a positive time with respect to the epoch.
 * The fractionalNanoseconds member is the fractional portion of the timestamp in units of 2^16 ns.
 * @verbatim For example:@endverbatim
 * @code
 * +2.000000001 seconds is represented by seconds = 0x0000 0000 0002
 * and fractionalNnanoseconds = 0x0000 0001 0000
 * @endcode
 *
 */
typedef struct ExtendedTimestamp {
	UInteger48m32 fractionalNanoseconds;
	UInteger48 seconds;
} ExtendedTimestamp;

#define VALUE_DISABLED 0
#define VALUE_ENABLED 1

/**
 * @breif the type of source of time used by a ClockMaster(802.1AS, 8.6.2.7 timeSource)
 */
typedef enum {
        ATOMIC_CLOCK         = 0x10,
        GPS                  = 0x20,
        TERRESTRIAL_RADIO    = 0x30,
        PTP                  = 0x40,
        NTP                  = 0x50,
        HAND_SET             = 0x60,
        OTHER                = 0x90,
        INTERNAL_OSCILLATOR  = 0xA0,
} TimeSource;

// 14.8.3 portState, (from IEEE 1588 Table-8)
/**
 * @brief value of the port state (802.1AS 14.8.3 portState)
 */
typedef enum {
	DisabledPort = 3,
	MasterPort = 6,
	PassivePort = 7,
	SlavePort = 9,
} PTPPortState;

#endif
/** @}*/
