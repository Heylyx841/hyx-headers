/**
 * @file   hyxlib.h
 *
 * @copyright
 *   Copyright (c) 2026 Heylyx841. All rights reserved.
 *   Licensed under the MIT License.
 *
 * @author   Heylyx841
 * @date     2026-02-01
 * @version  1.0.0
 *
 * @section DESCRIPTION
 *   本头文件作为 HYX 库的入口，提供编译期标准检测。
 *
 * @section USAGE
 *   直接包含此头文件即可：
 *   @code{.cpp}
 *   #include "hyxlib.h"
 *   @endcode
 */

#ifndef HYXLIB_H
#define HYXLIB_H

#ifdef __cplusplus

#if __cplusplus >= 202302L

#include "hyx_autoseq.hpp"

#endif

#endif

#endif // HYXLIB_H
