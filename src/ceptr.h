/**
 * @copyright Copyright (C) 2013-2015, The MetaCurrency Project (Eric Harris-Braun, Arthur Brock, et. al).  This file is part of the Ceptr platform and is released under the terms of the license contained in the file LICENSE (GPLv3).
 */

#ifndef _CEPTR_H
#define _CEPTR_H

#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>

// #include <pthread.h>
#include <stdlib.h>

#include "ceptr_error.h"
#include "tree.h"
#include "mtree.h"
#include "util.h"

#ifdef __MACH__
#include "mach_gettime.h"
#include "fmemopen.h"
#include <pthread.h>
#endif


#endif
