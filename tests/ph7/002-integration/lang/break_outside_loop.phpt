--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Break statement outside loop results in compile-time error
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
echo "Before break\n";
break;
echo "After break\n";
?>
--EXPECTF--
%s 3 Error: A 'break' statement may only be used within a loop or switch
Compile error
--CLEAN--
<?php

