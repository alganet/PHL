/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef PHL_SERVER_H
#define PHL_SERVER_H
#ifdef PHL_ENABLE_SERVER
/*
 * Start the PHL built-in development server.
 * Listens on zHost:iPort, serves files from zDocRoot.
 * If zRouter is non-NULL, it is used as the router script.
 * This function blocks until the server is shut down (e.g. via SIGINT).
 * Returns 0 on clean shutdown, non-zero on error.
 */
int phl_serve(const char *zHost, int iPort, const char *zDocRoot, const char *zRouter);
#endif /* PHL_ENABLE_SERVER */
#endif /* PHL_SERVER_H */
