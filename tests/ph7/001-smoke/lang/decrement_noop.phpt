--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Decrement on a non-numeric string or null is a no-op, and php diagnoses it
--FILE--
<?php
set_error_handler(function ($no, $msg) { echo "[$no] $msg\n"; return true; });
$decS = "abc"; $decS--; echo "[$decS]\n";
$decS = "a";   $decS--; echo "[$decS]\n";
$decS = "5x";  $decS--; echo "[$decS]\n";
$decN = null;  $decN--; echo ($decN === null ? 'null-noop' : 'CHANGED'), "\n";
$decI = null;  $decI++; echo var_export($decI, true), "\n";
restore_error_handler();
?>
--EXPECT--
[8192] Decrement on non-numeric string has no effect and is deprecated
[abc]
[8192] Decrement on non-numeric string has no effect and is deprecated
[a]
[8192] Decrement on non-numeric string has no effect and is deprecated
[5x]
[2] Decrement on type null has no effect, this will change in the next major version of PHP
null-noop
1
--CLEAN--
<?php
unset($decS, $decN, $decI);
