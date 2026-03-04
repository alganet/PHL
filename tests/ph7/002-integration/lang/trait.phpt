--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: trait keyword compile error
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
trait Hello {
    public function sayHello() {
        echo 'Hello ';
    }
}
?>
--EXPECTF--
%s 3 Error:  Unexpected token 'function'
Compile error
--CLEAN--
<?php

