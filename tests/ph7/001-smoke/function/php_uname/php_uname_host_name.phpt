--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php_uname names the host gethostname() names
--FILE--
<?php
var_dump(php_uname('n') === gethostname());
var_dump(php_uname('n') !== '');
var_dump(strpos(php_uname('a'), php_uname('n')) !== false);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
--CLEAN--
<?php
