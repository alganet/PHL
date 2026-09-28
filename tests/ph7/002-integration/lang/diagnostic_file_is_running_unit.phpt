--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A diagnostic names the file the running code is IN, not the top of the include stack
--FILE--
<?php
/* A diagnostic names the file the RUNNING code is in. That is the defining file of
 * the innermost active function, not the top of the include stack -- the two agree
 * only while top-level code is running, and once a call reaches a function defined
 * in another unit the include stack has moved on. Every warning any vendor package
 * raised therefore named the ENTRY SCRIPT, which is also what a framework's error
 * handler logged. */
set_error_handler(function ($n, $s, $f, $l) {
    printf("  handler %-24s %s:%d\n", $s, basename($f), $l);
    return true;
});
require __DIR__ . '/diagnostic_file_lib.inc';
echo "-- raised inside the included unit\n";
dfaWarn();
echo "-- ...and one call deeper\n";
dfaDeep();
echo "-- raised by top-level code of the entry script\n";
$b = []; $x = $b['absent'];
restore_error_handler();
echo "-- and the printed copy names it too\n";
dfaWarn();
--EXPECTF--
-- raised inside the included unit
  handler Undefined array key "absent" diagnostic_file_lib.inc:4
-- ...and one call deeper
  handler Undefined array key "absent" diagnostic_file_lib.inc:4
-- raised by top-level code of the entry script
  handler Undefined array key "absent" %s:18
-- and the printed copy names it too
PHP Warning:  Undefined array key "absent" in %sdiagnostic_file_lib.inc on line 4
