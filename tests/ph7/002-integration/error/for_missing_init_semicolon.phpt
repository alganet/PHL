--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
For loop invalid syntax in initialization
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
for($i=0$i<10; $i++) {
}
?>
--EXPECTF--
%s Fatal error:  Unexpected token '<' %s
--CLEAN--
<?php

