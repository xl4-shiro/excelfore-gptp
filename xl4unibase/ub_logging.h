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
 * @defgroup logging Control logging
 * @{
 * @file ub_logging.h
 * @copyright Copyright (C) 2019 Excelfore Corporation
 * @author Shiro Ninomiya (shiro@excelfore.com)
 *
 * @brief control logging by levels and categories.
 *
 * The logging is controlled by 8 levels which is from UBL_NONE to UBL_DEBUGV.
 * The levels are used, one is for regular console output,
 * and the other is for debug output.
 *
 * The two levels are set for each category.
 * The categories are defined by user applications, and need to be registered
 * by the initialization string.
 * Registered categories must be enumerated and managed by user.
 * Because category number 0 is used for 'unibase', in user applications
 * starting from category number 1 is recommended.
 */

#ifndef __UB_LOGGING_H_
#define __UB_LOGGING_H_

#include "inttypes.h"
#include "stdbool.h"

/**
 * @brief This defines different logging levels
 * - UBL_NONE No message will print on console
 * - UBL_FATAL to print FATAL message on console
 * - UBL_ERROR to print FATAL to ERROR message on console
 * - UBL_WARN to print FATAL to WARNING message on console
 * - UBL_INFO to print FATAL to INFO message on console
 * - UBL_INFOV to print FATAL to INFOV message on console
 * - UBL_DEBUG to print FATAL to DEBUG message on console
 * - UBL_DEBUGV to print FATAL to DEBUGV message on console
 */
typedef enum {
	UBL_NONE=0,
	UBL_FATAL=1,
	UBL_ERROR=2,
	UBL_WARN=3,
	UBL_INFO=4,
	UBL_INFOV=5,
	UBL_DEBUG=6,
	UBL_DEBUGV=7,
}ub_dbgmsg_level_t;


#ifndef MAX_LOGMSG_CATEGORIES
/** @brief maximum number of categories */
#define MAX_LOGMSG_CATEGORIES 16
#endif

/**
 * @brief 3-charcter string to represent each logging level
 */
#define DBGMSG_LEVEL_MARK {"NON", "FTL", "ERR", "WRN", "INF", "IFV", "DBG", "DBV"}

/**
 * @brief lowest 2 bits are used for TS Clock type, use ub_clocktype_t
 * 	UB_CLOCK_DEFAULT means no-timestamp
 */
#define UBL_TS_BIT_FIELDS 3 // bit0 and bit1

/**
 * @brief override valuse 'x' to the value of the environment variable 'y'
 * @note UBB_GETENV must be defined before this macro is called.
 */
#define UBL_OVERRIDE_ISTR(x,y) ub_log_initstr_override(x,UBB_GETENV(y))

#ifdef PRINT_FORMAT_NO_WARNING
/** @brief some compilers don't have this type of attribute */
#define PRINT_FORMAT_ATTRIBUTE1
#define PRINT_FORMAT_ATTRIBUTE4
#else
/** @brief let the compiler show warning when printf type format is wrong */
#define PRINT_FORMAT_ATTRIBUTE1 __attribute__((format (printf, 1, 2))) //!< format starts at 1st argument
#define PRINT_FORMAT_ATTRIBUTE4 __attribute__((format (printf, 4, 5))) //!< format starts at 4th argument
#endif

/**
 * @brief output to the console out, the arguments work like 'printf'
 */
int ub_console_print(const char * format, ...) PRINT_FORMAT_ATTRIBUTE1;

/**
 * @brief output to the debug out, the arguments work like 'printf'
 */
int ub_debug_print(const char * format, ...) PRINT_FORMAT_ATTRIBUTE1;

/**
 * @brief output to the both of console out and debug out
 */
int ub_console_debug_print(const char * format, ...) PRINT_FORMAT_ATTRIBUTE1;

/**
 * @brief output to console out and debug out by selection
 * @param console	select output to consol
 * @param debug	select output to debug memory
 */
int ub_console_debug_select_print(bool console, bool debug, const char * format, ...);


/**
 * @brief override 'istr' of ub_log_inig
 * @param ns	original 'istr'
 * @param os	overriding string
 * @return 'ns' or 'os'
 * @note if 'os' includes ',', the entire 'os' is returned and it overrides the entire 'istr'
 * 	if 'os' one module of 'ns' like 'mod1:55m', 'mod1' part is overridden by 'os'
 *	e.g. when ns="4,mod0:44,mod1:44", os="mod1:55m"
 *	ub_log_init(ub_log_initstr_override(ns, os)) is initialized by "4,mod0:44,mod1:55m"
 */
const char *ub_log_initstr_override(const char *ns, const char *os);

/**
 * @brief initialize logging by a string
 * @param istr	initialization string
 * @return 0 on success, -1 on error
 * @note an example of string is like "ubase:34r,mod1:45m,mod2:56g,mod3:23"
 * 	each category string is separated by a comma, and the category
 *	initialization is a way in 'ub_log_add_category'
 */
void ub_log_init(const char *istr);

/**
 * @brief add a category of logging at the bottom of the index
 * @param catstr	category initialization string
 * @return 0 on success, -1 on error
 * @note the string is like "catname:45m", 'catname' must be less than 7 characters,
 * 	4 is UBL_INFO levlel, 5 is UBL_INFOV level, 'm' is option to add monotonic
 *	clock timestamp.
 */
int ub_log_add_category(const char *catstr);

/**
 * @brief print log message
 * @param cat_index index of categories, the index is defined by the initialization
 *	string in unibase_init function
 * @param flags timestamp option is defined in lower 2 bits.
 * @param level log level, see ub_dbgmsg_level_t
 * @param *format any number of arguments like printf.
 * @return 0 on success, -1 on error
 */
int ub_log_print(int cat_index, int flags, ub_dbgmsg_level_t level,
		 const char * format, ...) PRINT_FORMAT_ATTRIBUTE4;

/**
 * @brief check if console log is enabled or not for the indicated cat_index and level
 * @return true if enabled, otherwise false.
 */
bool ub_clog_on(int cat_index, ub_dbgmsg_level_t level);

/**
 * @brief check if debug log is enabled or not for the indicated cat_index and level
 * @return true if enabled, otherwise false.
 */
bool ub_dlog_on(int cat_index, ub_dbgmsg_level_t level);

/**
 * @brief change console log level to clevel, and debug log level to delevl
 * @return 0 on success, -1 on error
 */
int ub_log_change(int cat_index, ub_dbgmsg_level_t clevel, ub_dbgmsg_level_t dlevel);

/**
 * @brief return console log level and console log level to the previous status
 * @return 0 on success, -1 on error
 */
int ub_log_return(int cat_index);

/**
 * @brief flush out messages on the both of cosole log and debug log
 * @note the function depends of the callback function
 */
void ub_log_flush(void);

#endif
/** @}*/
