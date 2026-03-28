--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Invalid goto usage
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test goto to non-existent label
goto nonexistent_label;

// Test goto inside try block (should warn in PHL)
try {
    goto inside_try;
} catch (Exception $e) {
    echo "Caught\n";
}
inside_try:

echo "Done\n";
?>
--EXPECTF--
%s Error:  goto inside try/catch block is disallowed %s
--CLEAN--
<?php

