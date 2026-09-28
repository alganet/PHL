--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A class constant may not name static:: ("static::" is not allowed in compile-time constants)
--FILE--
<?php
class CesA {
    const K = 7;
    const C = static::K;
}
echo "unreachable\n";
?>
--EXPECTF--
%A"static::" is not allowed in compile-time constants%A
--CLEAN--
<?php
