--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
undefined function call warning
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
nonexistent_function();
?>
--EXPECTF--
Warning: Call to undefined function 'nonexistent_function',NULL will be returned in %s on line %d
--CLEAN--
<?php

