--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
idate: Unknown format token produces warning
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$result = idate('q');
echo "Result: $result\n";
?>
--EXPECTF--
%s Warning:  idate(): Unknown date format token
Result: 0
--CLEAN--
<?php
unset($result);
