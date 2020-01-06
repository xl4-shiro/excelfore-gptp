/*
 * excelfore-gptp - Implementation of gPTP(IEEE 802.1AS)
 * Copyright (C) 2019 Excelfore Corporation (https://excelfore.com)
 *
 * This file is part of excelfore-gptp.
 *
 * excelfore-gptp is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * excelfore-gptp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with excelfore-gptp.  If not, see
 * <https://www.gnu.org/licenses/old-licenses/gpl-2.0.html>.
 */
/**
 * @defgroup unibase_macros utility macros
 * @{
 * @file unibase_macros.h
 * @author Shiro Ninomiya<shiro@excelfore.com>
 * @copyright Copyright (C) 2019 Excelfore Corporation
 * @brief utility macros for convenience
 *
 */
#ifndef __UNIBASE_MARCORS_H_
#define __UNIBASE_MARCORS_H_

/************************************************************
 * utility macros
 ************************************************************/
#define UB_SEC_NS 1000000000LL //!< one second in unit of nano second
#define UB_MSEC_NS 1000000 //!< one mili second in unit of nano second
#define UB_USEC_NS 1000 //!< one micro second in unit of nano second
#define UB_SEC_US 1000000LL //!< one second in unit of microsecond
#define UB_MSEC_US 1000 //!< one mili second in unit of microsecond
#define UB_SEC_MS 1000 //!< one second in unit of milisecond

#define UB_BIT(x) (1U<<(x)) //!< bit x
#define UB_MAX(x,y) ((x)>(y)?(x):(y)) //!< max(x,y)
#define UB_MIN(x,y) ((x)<(y)?(x):(y)) //!< min(x,y)

/**
 * @brief UB_LOG(level, formt, ...), level is compared to the level in the category
 *	which is defined by UB_LOGCAT.
 *
 * 	UB_LOGCAT must be defined in .c file to indicate the log category index.
 *	UB_LOGCAT=0 is reserved as this unibase category.
 *	e.g. UB_LOG(UBL_DEBUG, "%s:x=%d\n", __func__, x);
 *	if UBL_DEBUG<="the level in UB_LOGCAT", it is printed
 */
#define UB_LOG(args...) ub_log_print(UB_LOGCAT, 0, args)

/**
 * @brief UB_TLOG add timestamp regardless the timestamp option in the category
 */
#define UB_TLOG(args...) ub_log_print(UB_LOGCAT, UB_LOGTSTYPE, args)

/** @brief use this to print ub_streamid_t */
#define UB_PRIhexB8 "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X"
#define UB_ARRAY_B8(x) x[0],x[1],x[2],x[3],x[4],x[5],x[6],x[7] //!< used with UB_PRIhexB8
/** @brief use this to print ub_macaddr_t */
#define UB_PRIhexB6 "%02X:%02X:%02X:%02X:%02X:%02X"
#define UB_ARRAY_B6(x) x[0],x[1],x[2],x[3],x[4],x[5] //!< used with UB_PRIhexB6

/** @brief true if 6-byte binaries are not all zero */
#define UB_NON_ZERO_B6(x) ((x[0]|x[1]|x[2]|x[3]|x[4]|x[5])!=0)

/** @brief true if 8-byte binaries are not all zero */
#define UB_NON_ZERO_B8(x) ((x[0]|x[1]|x[2]|x[3]|x[4]|x[5]|x[6]|x[7])!=0)

/** @brief true if 6-byte binaries are all 0xff */
#define UB_ALLFF_B6(x) ((x[0]&x[1]&x[2]&x[3]&x[4]&x[5])==0xff)

/** @brief true if 8-byte binaries are all 0xff */
#define UB_ALLFF_B8(x) ((x[0]&x[1]&x[2]&x[3]&x[4]&x[5]&x[6]&x[7])==0xff)

/**
 * @brief two of inline functions: name_bit_field, name_set_bit_field
 *	are created for bit opperation on 8-bit variable.
 *
 *	e.g UB_BIT8_FIELD(bs, 2, 0x1f),
 *	  - bs_bit_field(&v): read 'bit6 to bit2 of 'v'
 *	  - bs_set_bit_field(&v, 5): set 'bit6 to bit2' of 'v' to 5
 */
#define UB_BIT8_FIELD(name, s, m)				      \
	static inline int name##_bit_field(uint8_t *x)		      \
	{ return (*x >> (s)) & (m); }				      \
	static inline int name##_set_bit_field(uint8_t *x, uint8_t v) \
	{ return (*x = (*x & ~((m) << (s))) | ((v & (m)) << (s))) ; }

/** @brief the same like UB_BIT8_FILED, works on 16-bit variable */
#define UB_BIT16_FIELD(name, s, m)					\
	static inline int name##_bit_field(uint16_t *x)			\
	{ return (htons(*x) >> (s)) & (m); }				\
	static inline int name##_set_bit_field(uint16_t *x, uint16_t v) \
	{return (*x = ntohs((htons(*x) & ~((m) << (s)))			\
				    | ((v & (m)) << (s))));}

/** @brief the same like UB_BIT8_FILED, works on 32-bit variable */
#define UB_BIT32_FIELD(name, s, m)					\
	static inline int name##_bit_field(uint32_t *x)			\
	{ return (htonl(*x) >> (s)) & (m); }				\
	static inline int name##_set_bit_field(uint32_t *x, uint32_t v) \
	{return (*x = ntohl((htonl(*x) & ~((m) << (s)))			\
				    | ((v & (m)) << (s))));}

/**
 * @brief inline function: name_toggle_bit_field
 *	is created for toggling bit opperation on 8-bit variable.
 *
 *	e.g UB_BIT8_TOGGLE_FIELD(bs, 2, 1),
 *	  - bs_toggle_bit_field(&v): toggle bit2 at each time of call
 */
#define UB_BIT8_TOGGLE_FIELD(name, s, m)				\
	static inline int name##_toggle_bit_field(uint8_t *x)		\
	{return (*x = (*x ^ ((m) << (s))));}

/** @brief the same like UB_BIT8_TOGGLE_FIELD, works on 16-bit variable */
#define UB_BIT16_TOGGLE_FIELD(name, s, m)				\
	static inline int name##_toggle_bit_field(uint16_t *x)		\
	{return (*x = ntohs((htons(*x) ^ ((m) << (s)))));}

/** @brief the same like UB_BIT8_TOGGLE_FIELD, works on 32-bit variable */
#define UB_BIT32_TOGGLE_FIELD(name, s, m)				\
	static inline int name##_toggle_bit_field(uint32_t *x)		\
	{return (*x = ntohl((htonl(*x) ^ ((m) << (s)))));}


/** @brief convert 'struct timespec' vaule to nanosecond integer */
#define UB_TS2NSEC(ts) ((uint64_t)((ts).tv_sec)*UB_SEC_NS+(ts).tv_nsec)

/** @brief convert 'struct timespec' vaule to microsecond integer */
#define UB_TS2USEC(ts) ((ts).tv_sec*UB_SEC_US+(ts).tv_nsec/UB_USEC_NS)

/** @brief convert 'struct timespec' vaule to milisecond integer */
#define UB_TS2MSEC(ts) ((ts).tv_sec*UB_SEC_MS+(ts).tv_nsec/UB_MSEC_NS)

/** @brief convert 'struct timeval' vaule to nanosecond integer */
#define UB_TV2NSEC(tv) ((uint64_t)((tv).tv_sec)*UB_SEC_NS+(tv).tv_usec*UB_USEC_NS)

/** @brief convert 'struct timeval' vaule to nanosecond integer */
#define UB_TV2USEC(tv) ((tv).tv_sec*UB_SEC_US+(tv).tv_usec)

/** @brief convert 'struct timeval' vaule to milisecond integer */
#define UB_TV2MSEC(tv) ((tv).tv_sec*UB_SEC_MS+(tv).tv_usec/UB_MSEC_US)

/** @brief convert nanosec value to 'struct timespec' vaule */
#define UB_NSEC2TS(ns, ts) {(ts).tv_sec=(ns)/UB_SEC_NS;(ts).tv_nsec=(ns)%UB_SEC_NS;}

/** @brief convert microsec value to 'struct timespec' vaule */
#define UB_USEC2TS(us, ts) {(ts).tv_sec=(us)/UB_SEC_US;(ts).tv_nsec=(us)%UB_SEC_US*UB_USEC_NS;}

/** @brief convert milisec value to 'struct timespec' vaule */
#define UB_MSEC2TS(ms, ts) {(ts).tv_sec=(ms)/UB_SEC_MS;(ts).tv_nsec=(ms)%UB_SEC_MS*UB_MSEC_NS;}

/** @brief convert nanosec value to 'struct timeval' vaule */
#define UB_NSEC2TV(ns, tv) {(tv).tv_sec=(ns)/UB_SEC_NS;(tv).tv_usec=(ns)%UB_SEC_NS/UB_USEC_NS;}

/** @brief convert microsec value to 'struct timeval' vaule */
#define UB_USEC2TV(us, tv) {(tv).tv_sec=(us)/UB_SEC_US;(tv).tv_usec=(us)%UB_SEC_US;}

/** @brief convert milisec value to 'struct timeval' vaule */
#define UB_MSEC2TV(ms, tv) {(tv).tv_sec=(ms)/UB_SEC_MS;(tv).tv_usec=(ms)%UB_SEC_MS*UB_MSEC_US;}

/** @brief tv1-tv2 in 64-bit nanosecond unit */
#define UB_TV_DIFF64NS(tv1,tv2) ((int64_t)(UB_TV2NSEC(tv1)-UB_TV2NSEC(tv2)))

/** @brief tv1+tv2 in 64-bit nanosecond unit */
#define UB_TV_ADD64NS(tv1,tv2) (UB_TV2NSEC(tv1)+UB_TV2NSEC(tv2))

/** @brief ts1-ts2 in 64-bit nanosecond unit */
#define UB_TS_DIFF64NS(ts1,ts2) ((int64_t)(UB_TS2NSEC(ts1)-UB_TS2NSEC(ts2)))

/** @brief ts1+ts2 in 64-bit nanosecond unit */
#define UB_TS_ADD64NS(ts1,ts2) (UB_TS2NSEC(ts1)+UB_TS2NSEC(ts2))

/** @brief rtv=tv1-tv2 in 64-bit nanosecond unit */
#define UB_TV_DIFF_TV(rtv,tv1,tv2) {				\
		int64_t ns_ub_m_=UB_TV_DIFF64NS(tv1,tv2);	\
		UB_NSEC2TV(ns_ub_m_,rtv);			\
	}

/** @brief rts=ts1-ts2 in 64-bit nanosecond unit */
#define UB_TS_DIFF_TS(rts,ts1,ts2) {				\
		int64_t ns_ub_m_=UB_TS_DIFF64NS(ts1,ts2);	\
		UB_NSEC2TS(ns_ub_m_,rts);			\
	}

/** @brief rtv=tv1+tv2 in 64-bit nanosecond unit */
#define UB_TV_ADD_TV(rtv,tv1,tv2) {				\
		int64_t ns_ub_m_=UB_TV_ADD64NS(tv1,tv2);	\
		UB_NSEC2TV(ns_ub_m_,rtv);			\
	}

/** @brief rts=ts1+ts2 in 64-bit nanosecond unit */
#define UB_TS_ADD_TS(rts,ts1,ts2) {				\
		int64_t ns_ub_m_=UB_TS_ADD64NS(ts1,ts2);	\
		UB_NSEC2TS(ns_ub_m_,rts);			\
	}

/**
 * @brief convert values between host and network byte order.
 * which converts the unsigned integer host long long from
 * host byte order to network byte order.
 */
#define UB_HTONLL(x) ((1==htonl(1)) ? (x) : ((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | \
		   htonl((x) >> 32))

/**
 * @brief convert values between host and network byte order.
 * converts the unsigned integer netlong from network byte
 * order to host byte order.
 */
#define UB_NTOHLL(x) ((1==ntohl(1)) ? (x) : ((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | \
		   ntohl((x) >> 32))

#endif
/** @}*/
