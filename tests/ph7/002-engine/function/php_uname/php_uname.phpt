--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php_uname test
--FILE--
<?php
$a = php_uname();
echo "php_uname=" . $a . PHP_EOL;
?>
--EXPECTF--
php_uname=%s
--CLEAN--
<?php
unset($a);
?>
