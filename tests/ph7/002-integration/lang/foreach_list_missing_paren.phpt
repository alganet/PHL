--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
foreach list() without parentheses produces compile error
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$rows = [[1,2]];
foreach ($rows as list) {
    echo "bad\n";
}
?>
--EXPECTF--
%s %s %s  foreach: %s
--CLEAN--
<?php
