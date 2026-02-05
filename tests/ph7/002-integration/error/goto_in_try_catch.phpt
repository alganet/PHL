--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: goto statement inside try/catch block is disallowed
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
try {
    goto label;
} catch (Exception $e) {
    echo "Caught exception\n";
}
label:
echo "Should not reach here\n";
?>
--EXPECTF--
%s 3 Error: goto inside try/catch block is disallowed
Compile error
--CLEAN--
<?php

