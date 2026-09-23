--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
chroot() refuses an argument its declared string cannot take
--DESCRIPTION--
chroot() is php's own `chroot(string $directory): bool` — the SKIPIF this test
used to carry, claiming it as a PHL extension absent from php, was wrong — and
it had no signature row, so nothing screened its argument: an array reached the
builtin, stringified to "Array", and the call answered false as if the path had
merely been unusable. With the row it answers php's TypeError.
--FILE--
<?php
try {
    var_dump(chroot(array()));
} catch (Throwable $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
?>
--EXPECT--
TypeError: chroot(): Argument #1 ($directory) must be of type string, array given
--CLEAN--
<?php
