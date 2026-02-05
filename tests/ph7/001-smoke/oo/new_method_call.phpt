--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Temporary object method invocation test
--FILE--
<?php
class NewMethodTest {
    public function greet() { echo "hello\n"; }
}

(new NewMethodTest())->greet();
?>
--EXPECT--
hello
--CLEAN--
<?php
// Nothing to clean

