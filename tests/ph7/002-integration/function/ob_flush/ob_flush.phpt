--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ob_flush: nested buffer flush test
--SKIPIF--
<?php
if (function_exists('zend_version')) { echo "skip: not PH7\n"; }
?>
--FILE--
<?php
ob_start();
echo 'p';
ob_start();
echo "c\n";
ob_flush();
$inner = ob_get_contents();
ob_end_clean();
$parent = ob_get_clean();
echo "inner=".$inner."\n";
echo "parent=".$parent."\n";
?>
--EXPECT--
c
inner=
parent=p
--CLEAN--
<?php
unset($inner, $parent);
