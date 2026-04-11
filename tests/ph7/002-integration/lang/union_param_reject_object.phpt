--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Union parameter: int|string rejects unrelated object
--FILE--
<?php
class UprBox {}
function upro_f(int|string $x) {}
try {
    upro_f(new UprBox());
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECTF--
upro_f(): Argument #1 ($x) must be of type string|int, UprBox given%A
--CLEAN--
<?php
