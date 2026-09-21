--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A builtin's warning carries php's "func(): " qualifier in the MESSAGE, not the header
--FILE--
<?php
// php puts the raising function's name in the message itself, so the user error
// handler, error_get_last() and the printed copy all agree. PHL used to add it
// only when printing, so a handler that matched on the function name never
// fired and error_get_last() answered a body php never produces.
set_error_handler(function ($n, $s) { echo "[$n] $s\n"; return true; });
trim("abc", "z..a");
unserialize("garbage");
// A builtin that spells the qualifier into its own text is not double-prefixed.
// (the errno TEXT is wildcarded: the Windows VFS leaves errno unset -- PLAN §7.4)
$f = fopen("/nonexistent/dir/x", "r");
restore_error_handler();

// The same text reaches error_get_last(), which is what php reports.
@trim("abc", "z..a");
$e = error_get_last();
echo $e['message'], "\n";
var_dump($e['type']);

// A handler that keys off the function name now works, as it does under php.
set_error_handler(function ($n, $s) {
    echo str_starts_with($s, "trim(): ") ? "matched\n" : "MISSED: $s\n";
    return true;
});
trim("abc", "z..a");
restore_error_handler();
?>
--EXPECTF--
[2] trim(): Invalid '..'-range, '..'-range needs to be incrementing
[2] unserialize(): Error at offset 0 of 7 bytes
[2] fopen(/nonexistent/dir/x): Failed to open stream: %A
trim(): Invalid '..'-range, '..'-range needs to be incrementing
int(2)
matched
--CLEAN--
<?php
