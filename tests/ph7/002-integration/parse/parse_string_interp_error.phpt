--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
"${...}" string interpolation is a hard parse error (php deprecates; PHL removes)
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip PHL removes what php only deprecates'; ?>
--FILE--
<?php
// php 8.2 DEPRECATES "${var}" string interpolation (still runs it) in favor of the
// canonical "{$var}"; PHL targets php's non-deprecated surface, so it is a hard
// parse error. The canonical "{$var}" form is unaffected.
$name = "world";
echo "Hello ${name}!";
?>
--EXPECTF--
%AParse error:%A"${" string interpolation was removed in php 8.2, use "{$...}" instead%A
--CLEAN--
<?php
unset($name);
