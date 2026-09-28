--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
quoted_printable_decode refuses an array and counts its arguments
--FILE--
<?php
foreach ([[[1]], [], [1, 2]] as $qpDecArg) {
    try {
        var_dump(quoted_printable_decode(...$qpDecArg));
    } catch (Throwable $qpDecArgErr) {
        echo get_class($qpDecArgErr), ": ", $qpDecArgErr->getMessage(), "\n";
    }
}
var_dump(quoted_printable_decode(123));
?>
--EXPECT--
TypeError: quoted_printable_decode(): Argument #1 ($string) must be of type string, array given
ArgumentCountError: quoted_printable_decode() expects exactly 1 argument, 0 given
ArgumentCountError: quoted_printable_decode() expects exactly 1 argument, 2 given
string(3) "123"
--CLEAN--
<?php
unset($qpDecArg, $qpDecArgErr);
