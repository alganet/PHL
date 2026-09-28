--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
gethostname answers the name the OS reports
--FILE--
<?php
// The name itself is the box's, so what is asserted is its SHAPE and that the
// call is stable -- never the bytes, which differ per machine.
$hostName = gethostname();
var_dump(is_string($hostName), $hostName !== '', $hostName === gethostname(), strpos($hostName, "\0") === false);
try { gethostname(1); } catch (Throwable $hostErr) { echo get_class($hostErr), ": ", $hostErr->getMessage(), "\n"; }
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
ArgumentCountError: gethostname() expects exactly 0 arguments, 1 given
--CLEAN--
<?php
unset($hostName, $hostErr);
