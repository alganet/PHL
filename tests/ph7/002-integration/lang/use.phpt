--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: use statement (disabled feature)
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
use MyNamespace\MyClass;
echo "use test\n";
?>
--EXPECTF--
%s 2 Notice: Namespace support is disabled in the current release of the PH7(2.1.4) engine
use test
--CLEAN--
<?php

