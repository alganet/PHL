--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Union variadic: int|string ...$xs rejects array element
--FILE--
<?php
function uvr_f(int|string ...$xs) {}
try {
    uvr_f(1, "x", [1, 2]);
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECTF--
uvr_f(): Argument #%d%Amust be of type string|int, array given%A
--CLEAN--
<?php
