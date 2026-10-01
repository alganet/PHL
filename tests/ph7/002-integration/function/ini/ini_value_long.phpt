--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A php.ini value longer than a few hundred bytes is stored whole
--INI--
user_agent=Lorem0123456789abcdefghij0123456789abcdefghij0123456789abcdefghij0123456789abcdefghij0123456789abcdefghij0123456789abcdefghij0123456789abcdefghij0123456789abcdefghij0123456789abcdefghij0123456789abcdefghij0123456789abcdefghij0123456789abcdefghij0123456789abcdefghij0123456789abcdefghij0123456789abcdefghij0123456789abcdefghij0123456789abcdefghij0123456789abcdefghij0123456789abcdefghij0123456789abcdefghij0123456789abcdefghij0123456789abcdefghij0123456789abcdefghij0123456789abcdefghij0123456789abcdefghij0123456789abcdefghij0123456789abcdefghij0123456789abcdefghij0123456789abcdefghij0123456789abcdefghij0123456789abcdefghij0123456789abcdefghij0123456789abcdefghijEnd
--FILE--
<?php
// php puts no ceiling on an ini value and a real one can be long: PHPUnit hands
// its child a ~700-byte redaction pattern through -d.
$v = ini_get('user_agent');
var_dump(strlen($v));
var_dump(substr($v, 0, 5), substr($v, -3));
?>
--EXPECT--
int(668)
string(5) "Lorem"
string(3) "End"
