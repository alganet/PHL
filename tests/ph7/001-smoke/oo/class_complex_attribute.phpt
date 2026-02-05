--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Class with complex attribute
--FILE--
<?php
class TestClass {
    public $value = 25 << 1 + 3;
}

$instance = new TestClass();
echo $instance->value;
?>
--EXPECT--
400
--CLEAN--
<?php
unset($instance);
