--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php_uname assembles its composite in php order
--FILE--
<?php
// The bytes are the box's; the ORDER is php's -- system, host, release,
// version, machine. This engine used to put the host name fourth.
$unameParts = [php_uname('s'), php_uname('n'), php_uname('r'), php_uname('v'), php_uname('m')];
var_dump(php_uname('a') === implode(' ', $unameParts));
var_dump(php_uname() === php_uname('a'));
var_dump(count(array_filter($unameParts, 'is_string')));
?>
--EXPECT--
bool(true)
bool(true)
int(5)
--CLEAN--
<?php
unset($unameParts);
