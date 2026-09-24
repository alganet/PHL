--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: the read-STDIN-until-feof loop terminates on exactly php's iteration count
--STDIN--
alpha
beta
gamma
--FILE--
<?php
/* php://stdin's device reported a zero-byte read — its end of file — as an IO
 * ERROR, so nothing could ever latch EOF and this loop, the most ordinary thing
 * a CLI filter does, ran forever. The counter is bounded so a regression FAILS
 * here instead of hanging the suite. */
$lines = [];
$iters = 0;
while (!feof(STDIN)) {
    $line = fgets(STDIN);
    $iters++;
    if ($line !== false) { $lines[] = rtrim($line, "\n"); }
    if ($iters > 20) { echo "RUNAWAY\n"; break; }
}
echo 'iterations: ', $iters, "\n";
echo 'lines: ', implode('|', $lines), "\n";
echo 'eof: ', var_export(feof(STDIN), true), "\n";
?>
--EXPECT--
iterations: 3
lines: alpha|beta|gamma
eof: true
