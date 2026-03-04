--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Goto inside try/catch block results in compile-time error
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
try {
    goto label;
    echo "not reached\n";
} catch (Exception $e) {
    echo "caught\n";
}
label:
echo "label\n";
?>
--EXPECTF--
%s 3 Error:  goto inside try/catch block is disallowed
Compile error
--CLEAN--
<?php

