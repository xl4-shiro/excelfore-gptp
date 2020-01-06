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
 * @defgroup strutils String Utilities
 * @{
 * @file ub_strutils.h
 * @copyright Copyright (C) 2019 Excelfore Corporation
 * @author Shiro Ninomiya (shiro@excelfore.com)
 *
 * @brief utility functions for strings
 *
 */

#ifndef __UB_STRUTILS_H_
#define __UB_STRUTILS_H_

typedef uint8_t ub_bytearray8_t[8]; //!< 8-byte binary array
typedef ub_bytearray8_t ub_streamid_t; //!< 8-byte binary array is used for Stream ID

typedef uint8_t ub_bytearray6_t[6]; //!< 6-byte binary array
typedef ub_bytearray6_t ub_macaddr_t; //!< 6-byte binary array is used for MAC Address


/**
 * @brief dump out data in hex format to the console
 * @param buf	reference to the data buffer
 * @param size	size of the data to be dumped.
 * @param addr	offset address at the top of the data
 */
void ub_hexdump(unsigned char *buf, int size, int addr);

/**
 * @brief convert a binary type mac address to a string type mac
 * address('XX:XX:XX:XX:XX:XX' format)
 * @return mac address in string format, the same pointer as smac
 * @param bmac	mac address in binary format.
 * @param smac	the converted mac addrsss.
 * @note smac must have at least 18 bytes.
*/
char *ub_bmac2smac(ub_macaddr_t bmac, char *smac);

/**
 * @brief convert a string type mac address('XX:XX:XX:XX:XX:XX' format)
 *  to a binary type mac address.
 * @return a reference of the mac address in binary format, the same pointer as bmac.
 * @param smac	mac address in string format
 * @param bmac	the converted mac address in binary format
 */
uint8_t *ub_smac2bmac(const char *smac, ub_macaddr_t bmac);

/**
 * @brief convert a binary type stream id to a string type stream id
 *  ('XX:XX:XX:XX:XX:XX:XX:XX' format)
 * @return the converted stream id in string format, the same pointer as sidstr
 * @param bsid	stream id in binary format
 * @param ssid the converted stream id in string format.
 * @note ssid must have at least 24 bytes
 */
char *ub_bsid2ssid(ub_streamid_t bsid, char *ssid);

/**
 * @brief convert a string type stream id('XX:XX:XX:XX:XX:XX:XX:XX' format)
 *   to a binary type stream id
 * @return a reference of the mac address in binary format, the same pointer as bsid
 * @param ssid	stream id in string format; either lower case format or upper scale format
 * @param bsid	the converted stream id in binary format
 */
uint8_t *ub_ssid2bsid(const char *ssid, ub_streamid_t bsid);

/**
 * @brief look for non space charcter and return the number of passed characters
 * @param astr	string
 * @param maxn	max number of characters to be checked
 * @note whitespace is either of space,tab,CR,LF
 * @return the index of the first non-whitespace charcter
 */
int ub_find_nospace(const char *astr, int maxn);

/**
 * @brief look for space charcter and return the number of passed characters
 * @return the index of the first whitespace charcter appears.
 * @param astr	string
 * @param maxn	max number of characters to be checked
 * @note whitespace is either of space,tab,CR,LF \n
 * whitespaces quoted by ' or " are skipped, and they can be escaped with back slash
 */
int ub_find_space(const char *astr, int maxn);

/**
 * @brief parse a command line string and make argc,argv[] for main function,
 * @return the number of parameters (argc)
 * @param line	command line string
 * @param argv	the result of argment array
 * @param maxargc	maximum number of arguments
 */
int ub_command_line_parser(char *line, char *argv[], int maxargc);

#endif
/** @}*/
