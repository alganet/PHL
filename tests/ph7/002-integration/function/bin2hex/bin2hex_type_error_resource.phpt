--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Passing a resource to bin2hex should raise a TypeError
--FILE--
<?php
$fp = fopen(__FILE__, 'r');
bin2hex($fp);
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: bin2hex(): Argument #1 ($string) must be of type string, resource given in %s
--CLEAN--
<?php
unset($fp);
