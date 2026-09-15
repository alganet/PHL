--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
stripslashes with null argument coerces to the empty string
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip php only deprecates null here, and the in-process runner surfaces it'; ?>
--FILE--
<?php
$result = stripslashes(null);
var_dump($result);
?>
--EXPECT--
string(0) ""
--CLEAN--
<?php
unset($result);
