--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
inet_pton refuses an array and a NUL-bearing address
--FILE--
<?php
foreach ([[[1]], [], [1, 2], ["1.2.3.4\x00x"]] as $ptonArg) {
    try {
        var_dump(inet_pton(...$ptonArg));
    } catch (Throwable $ptonArgErr) {
        echo get_class($ptonArgErr), ": ", $ptonArgErr->getMessage(), "\n";
    }
}
?>
--EXPECT--
TypeError: inet_pton(): Argument #1 ($ip) must be of type string, array given
ArgumentCountError: inet_pton() expects exactly 1 argument, 0 given
ArgumentCountError: inet_pton() expects exactly 1 argument, 2 given
ValueError: inet_pton(): Argument #1 ($ip) must not contain any null bytes
--CLEAN--
<?php
unset($ptonArg, $ptonArgErr);
