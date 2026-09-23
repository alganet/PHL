<?php
/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Pick a TCP port on 127.0.0.1 that nothing is listening on right now.
 *
 * The server tests used to derive their port from the pid alone
 * (`$base + getmypid() % 100`), which collides with whatever else happens to be
 * listening on this machine. When it does, the test's own `-S` never binds and
 * curl talks to the FOREIGN listener instead — and with no timeout on either
 * side that is a HANG, not a failure: one such collision (a port an editor held)
 * stalled a whole gate run for half an hour. Probing first turns the collision
 * into a different port, and the callers' `--max-time` turns any remaining
 * surprise into a failed assertion rather than a stuck suite.
 */
function phpt_free_port($base = 19500, $span = 100)
{
    $port = $base + (getmypid() % $span);
    for ($i = 0; $i < $span; $i++) {
        $probe = @fsockopen('127.0.0.1', $port, $errno, $errstr, 0.2);
        if ($probe === false) {
            return $port;   /* nothing accepted a connection: free */
        }
        fclose($probe);
        $port = $base + (($port - $base + 1) % $span);
    }
    return $port;           /* every candidate busy: let the test report it */
}
