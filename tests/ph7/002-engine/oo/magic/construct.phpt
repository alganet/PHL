--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Magic method __construct
--FILE--
<?php
class FooConstruct {
    public function __construct($a = 0){
        echo "constructed: $a\n";
    }
}

$o = new FooConstruct(7);
// also test default arg
$d = new FooConstruct();
?>
--EXPECT--
constructed: 7
constructed: 0

--CLEAN--
<?php
unset($o,$d);
?>
