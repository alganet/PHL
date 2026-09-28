--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
quoted_printable_encode refuses an array and counts its arguments
--FILE--
<?php
foreach ([[[1]], [], [1, 2]] as $qpEncArg) {
    try {
        var_dump(quoted_printable_encode(...$qpEncArg));
    } catch (Throwable $qpEncArgErr) {
        echo get_class($qpEncArgErr), ": ", $qpEncArgErr->getMessage(), "\n";
    }
}
var_dump(quoted_printable_encode(123));
?>
--EXPECT--
TypeError: quoted_printable_encode(): Argument #1 ($string) must be of type string, array given
ArgumentCountError: quoted_printable_encode() expects exactly 1 argument, 0 given
ArgumentCountError: quoted_printable_encode() expects exactly 1 argument, 2 given
string(3) "123"
--CLEAN--
<?php
unset($qpEncArg, $qpEncArgErr);
