--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Magic method __destruct
--FILE--
<?php
class FooDestruct {
    public function __destruct(){
        echo "destructed\n";
    }
}

$obj = new FooDestruct();
unset($obj); // ensure destructor is invoked
?>
--EXPECT--
destructed
