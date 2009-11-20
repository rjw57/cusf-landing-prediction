/* system-info.c Functions to get various bits of system information. */

/* FIRTREE - A generic image processing library
 * Copyright (C) 2009 Rich Wareham <richwareham@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "system-info.h"

/* These are the magic includes required for system_info_cpu_cores(). */
#ifdef WIN32
#include "windows.h"
#elif defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysctl.h>
#else
#include <unistd.h> 
#endif

/* If you have more cores than this then you have enough money to
 * pay me to change this macro. */
#define MAX_THREADS 8

/*=========================================================================*/
/* This is shamelessly lifted from the wonderful Blender 
 * (http://blender.org). Specifically it comes from 
 * blender/source/blender/blenlib/intern/threads.c */
int
system_info_cpu_cores()
{	
	int t;
#ifdef WIN32
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	t = (int) info.dwNumberOfProcessors;
#else 
#	ifdef __APPLE__
	int mib[2];
	size_t len;
	
	mib[0] = CTL_HW;
	mib[1] = HW_NCPU;
	len = sizeof(t);
	sysctl(mib, 2, &t, &len, NULL, 0);
#	elif defined(__sgi)
	t = sysconf(_SC_NPROC_ONLN);
#	else
	t = (int)sysconf(_SC_NPROCESSORS_ONLN);
#	endif
#endif
	
	if (t>MAX_THREADS)
		return MAX_THREADS;
	if (t<1)
		return 1;
	
	return t;
}

/* vim:cindent:sw=4:ts=4:et
 */
