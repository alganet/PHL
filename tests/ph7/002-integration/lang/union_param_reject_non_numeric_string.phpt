--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Union parameter: int|float rejects non-numeric string
--FILE--
<?php
function uprnns_f(int|float $x) {}
try {
    uprnns_f("abc");
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECTF--
uprnns_f(): Argument #1 ($x) must be of type int|float, string given%A
--CLEAN--
<?php
