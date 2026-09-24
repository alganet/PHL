--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
an UNCAUGHT throw from a builtin's callback stops the builtin and is reported ONCE — not once per remaining element
--FILE--
<?php
// The uncaught half of the same rule. It comes back to the builtin as a
// different status than a caught throw does (the fatal has already been
// reported), and a loop that only understood the caught one ran the callback
// again for every remaining element -- repeating its SIDE EFFECTS and printing
// the fatal once per element. Asserted through a child interpreter, because the
// event terminates the script; the trace body is wildcarded because php names
// the internal frame and PHL does not (a separate, recorded divergence).
$cbx_f = tempnam(sys_get_temp_dir(), 'cbx');
file_put_contents($cbx_f, <<<'CODE'
<?php
$log = [];
register_shutdown_function(function () { global $log; echo "calls=", count($log), "\n"; });
array_map(function ($v) { global $log; $log[] = $v; throw new RuntimeException("stop"); }, [1, 2, 3]);
echo "NEVER\n";
CODE);
$cbx_out = shell_exec(escapeshellarg(PHP_BINARY) . ' ' . escapeshellarg($cbx_f) . ' 2>&1');
unlink($cbx_f);
// One report, one call, and the statement after the builtin never runs.
echo 'reports=', substr_count((string)$cbx_out, 'Uncaught RuntimeException: stop'), "\n";
echo 'never=', substr_count((string)$cbx_out, 'NEVER'), "\n";
echo trim(substr((string)$cbx_out, strrpos((string)$cbx_out, 'calls='))), "\n";
?>
--EXPECT--
reports=1
never=0
calls=1
--CLEAN--
<?php
unset($cbx_f, $cbx_out);
