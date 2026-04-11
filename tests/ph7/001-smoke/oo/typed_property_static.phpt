--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: static
--FILE--
<?php
class TpsCounter {
    public static int $total = 0;
    public static function bump(): int { return ++self::$total; }
}
TpsCounter::bump();
TpsCounter::bump();
TpsCounter::bump();
echo TpsCounter::$total, "\n";
?>
--EXPECT--
3
--CLEAN--
<?php
