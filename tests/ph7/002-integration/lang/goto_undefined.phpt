--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
goto statement with undefined label
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
goto MISSING_LABEL;
echo "This should not execute\n";
UNDEFINED_LABEL:
echo "Defined label\n";
?>
--EXPECTF--
%s %d Error: Label 'MISSING_LABEL' was referenced but not defined
%s %d Warning: Label 'UNDEFINED_LABEL' is defined but not referenced
Compile error
--CLEAN--
<?php

