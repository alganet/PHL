--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
chroot() with no arguments is php's ArgumentCountError
--DESCRIPTION--
The signature row that gives chroot() its `string $directory` also gives it its
arity. It had neither, so a bare chroot() ran the builtin with no arguments and
answered false.
--FILE--
<?php
try {
    var_dump(chroot());
} catch (Throwable $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
?>
--EXPECT--
ArgumentCountError: chroot() expects exactly 1 argument, 0 given
--CLEAN--
<?php
