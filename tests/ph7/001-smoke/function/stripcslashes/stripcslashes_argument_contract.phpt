--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
stripcslashes refuses an array and counts its arguments
--FILE--
<?php
foreach ([[[1]], [], [1, 2]] as $scArg) {
    try {
        var_dump(stripcslashes(...$scArg));
    } catch (Throwable $scArgErr) {
        echo get_class($scArgErr), ": ", $scArgErr->getMessage(), "\n";
    }
}
var_dump(stripcslashes(123));
?>
--EXPECT--
TypeError: stripcslashes(): Argument #1 ($string) must be of type string, array given
ArgumentCountError: stripcslashes() expects exactly 1 argument, 0 given
ArgumentCountError: stripcslashes() expects exactly 1 argument, 2 given
string(3) "123"
--CLEAN--
<?php
unset($scArg, $scArgErr);
