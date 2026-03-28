--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Goto a missing label results in compile-time error
--SKIPIF--
<?php if (function_exists('zend_version')) { echo "skip"; } ?>
--FILE--
<?php
goto missing_label;
?>
--EXPECTF--
%s Error:  Label 'missing_label' was referenced but not defined %s
--CLEAN--
<?php

