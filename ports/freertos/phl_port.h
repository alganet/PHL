/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef PHL_PORT_H
#define PHL_PORT_H

#include "ph7.h"

/*
 * Initialize PHL for FreeRTOS: registers custom memory allocator
 * and configures the engine for embedded operation.
 * Must be called before any ph7_init() call.
 */
int phl_port_init(void);

/*
 * VM output consumer callback suitable for PH7_VM_CONFIG_OUTPUT.
 * Appends output to a SyBlob passed via pUserData.
 */
int phl_port_output_consumer(const void *pOutput, unsigned int nOutputLen, void *pUserData);

/*
 * Compile and execute a PHP script from a string.
 * Stores the output in zOut (up to nOut bytes).
 * Returns PH7_OK on success.
 */
int phl_port_exec(const char *zScript, char *zOut, unsigned int nOut, unsigned int *pOutLen);

#endif /* PHL_PORT_H */
