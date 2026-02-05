--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Access attribute on a fresh temporary object (new Test())->attr
--FILE--
<?php
class NewAttrTest {
    public $foo = 'bar';
}
// Access attribute directly on a temporary instance: (new NewAttrTest())->foo
echo (new NewAttrTest())->foo . "\n";
?>
--EXPECT--
bar
--CLEAN--
<?php

