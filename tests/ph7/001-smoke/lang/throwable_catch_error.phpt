--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Throwable: catch Error via Throwable
--FILE--
<?php
try {
    throw new Error("engine");
} catch (Throwable $t) {
    echo get_class($t), ":", $t->getMessage(), "\n";
}
?>
--EXPECT--
Error:engine
--CLEAN--
<?php
