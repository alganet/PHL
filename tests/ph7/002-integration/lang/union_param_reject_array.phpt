--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Union parameter: int|string rejects array with TypeError
--FILE--
<?php
function upra_f(int|string $x) {}
try {
    upra_f([1, 2, 3]);
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECTF--
upra_f(): Argument #1 ($x) must be of type string|int, array given%A
--CLEAN--
<?php
