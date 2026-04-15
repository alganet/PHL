--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Throwable: catch user Exception via Throwable
--FILE--
<?php
try {
    throw new Exception("boom");
} catch (Throwable $t) {
    echo get_class($t), ":", $t->getMessage(), "\n";
}
?>
--EXPECT--
Exception:boom
--CLEAN--
<?php
