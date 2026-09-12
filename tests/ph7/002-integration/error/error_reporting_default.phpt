--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Default error_reporting() level is E_ALL (30719) — the removed E_STRICT bit (2048) is not included (php 8)
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip the startup level is php.ini configuration, not engine behaviour'; ?>
--FILE--
<?php
echo error_reporting(), "\n";
echo E_ALL, "\n";
var_dump(error_reporting() === E_ALL);
?>
--EXPECT--
30719
30719
bool(true)
