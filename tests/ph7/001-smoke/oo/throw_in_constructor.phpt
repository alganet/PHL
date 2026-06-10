--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: Throw from a class constructor and catch it
--FILE--
<?php
class TestCtorThrow {
    public function __construct(){
        throw new Exception("ctor fail");
    }
}

try {
    $o = new TestCtorThrow();
    echo "no\n"; // must NOT run: the constructor threw and unwound
} catch (Exception $e) {
    echo "caught: " . $e->getMessage() . "\n";
}
?>
--EXPECT--
caught: ctor fail
--CLEAN--
<?php
// $o was never assigned because the constructor threw.
