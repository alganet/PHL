--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Leading-numeric string in arithmetic warns and uses the numeric prefix
--FILE--
<?php
set_error_handler(function ($no, $msg) { echo "Warning: $msg\n"; return true; });
$numcS = "10abc";
echo ($numcS + 5), "\n";
$numcS = "  -3.5e1foo";
echo ($numcS + 0), "\n";
restore_error_handler();
?>
--EXPECT--
Warning: A non-numeric value encountered
15
Warning: A non-numeric value encountered
-35
--CLEAN--
<?php
unset($numcS);
