--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
implode with empty items
--FILE--
<?php
$arr = array('foo', '', 'baz');
echo implode('.', $arr) . "\n";
?>
--EXPECT--
foo..baz
--CLEAN--
<?php
unset($arr);
