--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: yield keyword compile error
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
function gen() {
    yield 1;
}
?>
--EXPECTF--
%s Error:  Unexpected token '1' %s
--CLEAN--
<?php

