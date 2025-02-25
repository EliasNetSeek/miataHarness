/*
 * @file global.h
 *
 * Global utility header file for firmware
 *
 * Simulator and unit tests have their own version of this header
 *
 * @date May 27, 2013
 * @author Andrey Belomutskiy, (c) 2012-2020
 */

#pragma once

// todo: remove this from here and rely on os_access.h. unfortunately hal.h includes ch.h :(
#include <hal.h>
// *** IMPORTANT *** from painful experience we know that common_headers.h has to be included AFTER hal.h
// *** https://github.com/rusefi/rusefi/issues/1007 ***
#include "common_headers.h"

// for US_TO_NT_MULTIPLIER
#include "mpu_util.h"

// this is about MISRA not liking 'time.h'. todo: figure out something
#if defined __GNUC__
// GCC
#include <sys/types.h>
#else
// IAR
typedef unsigned int time_t;
#endif

#ifdef __cplusplus
#include "eficonsole.h"
#include <ch.hpp>
#endif /* __cplusplus */


/* definition to expand macro then apply to pragma message */
#define VALUE_TO_STRING(x) #x
#define VALUE(x) VALUE_TO_STRING(x)
#define VAR_NAME_VALUE(var) #var "="  VALUE(var)

#define CORE_CLOCK STM32_SYSCLK
//#pragma message(VAR_NAME_VALUE(CORE_CLOCK))


/**
 * project-wide default thread stack size
 * See also PORT_INT_REQUIRED_STACK
 * See getRemainingStack()
 * See CountFreeStackSpace()
 * See "threadsinfo" command cmd_threads
 */
#ifndef UTILITY_THREAD_STACK_SIZE
#define UTILITY_THREAD_STACK_SIZE 400
#endif /* UTILITY_THREAD_STACK_SIZE */

// see also validateStack
#define getCurrentRemainingStack() getRemainingStack(chThdGetSelfX())

#define EFI_ERROR_CODE 0xffffffff

#include "global_port.h"

#define UNIT_TEST_BUSY_WAIT_CALLBACK() {}
