/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
 * @file rohc_traces.h
 * @brief ROHC macros for traces
 * @author Didier Barvaux <didier.barvaux@toulouse.viveris.com>
 */

#ifndef ROHC_TRACES_H
#define ROHC_TRACES_H

#include <stdio.h>
#include "config.h" /* for ROHC_DEBUG_LEVEL */

/// @brief Print information depending on the debug level and prefixed
///        with the function name
#define rohc_debugf(level, format, ...) \
	rohc_debugf_(level, "%s[%s:%d %s()] " format, \
	             (level == 0 ? "[ERROR] " : ""), \
	             __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)

/// Print information depending on the debug level
#define rohc_debugf_(level, format, ...) \
	do { \
		if((level) <= ROHC_DEBUG_LEVEL) \
			printf(format, ##__VA_ARGS__); \
	} while(0)


#endif
