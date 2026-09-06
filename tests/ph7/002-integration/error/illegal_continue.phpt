--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: continue statement outside loop
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
continue;
?>
--EXPECTF--
%AFatal error:%A'continue' not in the 'loop' or 'switch' context%A
--CLEAN--
<?php

