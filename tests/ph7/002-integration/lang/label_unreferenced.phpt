--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Unreferenced label emits warning
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
my_label:
echo "test\n";
?>
--EXPECTF--
%s 2 Warning: Label 'my_label' is defined but not referenced
test
--CLEAN--
<?php

