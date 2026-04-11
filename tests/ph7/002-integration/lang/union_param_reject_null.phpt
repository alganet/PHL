--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Union parameter: int|string rejects null when not nullable
--FILE--
<?php
function uprnull_f(int|string $x) {}
try {
    uprnull_f(null);
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECTF--
uprnull_f(): Argument #1 ($x) must be of type string|int, null given%A
--CLEAN--
<?php
