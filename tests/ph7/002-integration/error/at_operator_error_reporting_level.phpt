--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: the @ operator lowers error_reporting() so a handler can honor it
--FILE--
<?php
error_reporting(E_ALL);
$atSeen = [];
set_error_handler(function($no, $str) use (&$atSeen) {
    if (!(error_reporting() & $no)) { return false; } // respect the @ operator
    $atSeen[] = $str;
    return true;
});
echo "outside=", error_reporting(), "\n";
$atInside = null;
@(function() use (&$atInside) { $atInside = error_reporting(); })();
echo "inside=", $atInside, "\n";
// a suppressed warning must not reach a handler that honors error_reporting()
@file('no-such-file-atrep.php');
echo "seen-after-suppressed=", count($atSeen), "\n";
// an unsuppressed one is counted
@file('no-such-file-atrep.php') ?: file('no-such-file-atrep.php');
echo "seen-after-plain=", count($atSeen), "\n";
echo "outside-again=", error_reporting(), "\n";
restore_error_handler();
?>
--EXPECT--
outside=30719
inside=4437
seen-after-suppressed=0
seen-after-plain=1
outside-again=30719
