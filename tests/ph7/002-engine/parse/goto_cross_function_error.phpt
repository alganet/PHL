--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
goto label unreachable across functions
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
function foo() {
    LABEL:
    echo "in foo\n";
}

goto LABEL;
?>
--EXPECTF--
%s %d Warning: Label 'LABEL' is defined but not referenced
%s %d Error: Label 'LABEL' is unreachable
Compile error
