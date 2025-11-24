--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
vsprintf formats a string using arguments array
--FILE--
<?php
$ret = vsprintf('%s-%d', array('a', 1));
echo $ret . "\n";
?>
--EXPECT--
a-1
--CLEAN--
<?php
unset($ret);
?>
