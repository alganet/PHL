--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: break statement outside loop or switch
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
break;
?>
--EXPECTF--
%AFatal error:%A'break' not in the 'loop' or 'switch' context%A
--CLEAN--
<?php

