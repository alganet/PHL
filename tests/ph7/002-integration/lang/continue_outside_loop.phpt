--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Continue statement outside loop results in compile-time error
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
echo "Before continue\n";
continue;
echo "After continue\n";
?>
--EXPECTF--
%s 3 Error:  A 'continue' statement may only be used within a loop or switch
Compile error
--CLEAN--
<?php

