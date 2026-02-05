--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
FNM_NOESCAPE constant value (PH7 specific)
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
echo "FNM_NOESCAPE value: " . FNM_NOESCAPE . "\n";
?>
--EXPECT--
FNM_NOESCAPE value: 1
--CLEAN--
<?php

