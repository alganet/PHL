--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
strpos with empty needle
--FILE--
<?php
var_dump(strpos("hello world", ""));
?>
--EXPECT--
bool(FALSE)
--CLEAN--
<?php

