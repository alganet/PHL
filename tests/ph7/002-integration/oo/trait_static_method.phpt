--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Trait with static method and property
--FILE--
<?php
trait Counter {
    public static $count = 0;
    public static function increment() {
        self::$count++;
        return self::$count;
    }
}
class MyClass {
    use Counter;
}
echo MyClass::increment(), "\n";
echo MyClass::increment(), "\n";
echo MyClass::$count, "\n";
?>
--EXPECT--
1
2
2
--CLEAN--
<?php
