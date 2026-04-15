--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Throwable: throwing a non-Throwable raises Error
--FILE--
<?php
class NtPlain { public $x = 1; }
try {
    throw new NtPlain();
} catch (Throwable $t) {
    echo get_class($t), ":", $t->getMessage(), "\n";
}
?>
--EXPECT--
Error:Cannot throw objects that do not implement Throwable
--CLEAN--
<?php
