--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The uncaught report names the file the exception was RAISED in, not the entry script
--DESCRIPTION--
php's zend_exception_error reports the throwable's own file/line, so an exception
that escapes a library names that library's file however many frames it unwound
through. PHL read the top of the include stack, which is the entry script once
anything defined elsewhere is running -- so every uncaught report in a composer
tree named the wrong file. Both links of a $previous chain report their own.
--FILE--
<?php
$dir = sys_get_temp_dir() . '/phl_uncaught_attr';
@mkdir($dir);
$lib = $dir . '/uncaught_attr_lib.php';
file_put_contents($lib, "<?php\n"
	. "function uncaughtAttrInner() { throw new RuntimeException('inner'); }\n"
	. "function uncaughtAttrOuter() {\n"
	. "	try { uncaughtAttrInner(); }\n"
	. "	catch (RuntimeException \$e) { throw new LogicException('outer', 0, \$e); }\n"
	. "}\n");
require $lib;
uncaughtAttrOuter();
?>
--EXPECTF--
%APHP Fatal error:  Uncaught RuntimeException: inner in %suncaught_attr_lib.php:2
Stack trace:
#0 %s(%d): uncaughtAttrInner()
#1 %s(%d): uncaughtAttrOuter()
#2 {main}

Next LogicException: outer in %suncaught_attr_lib.php:5
Stack trace:
#0 %s(%d): uncaughtAttrOuter()
#1 {main}
  thrown in %suncaught_attr_lib.php on line 5%A
--CLEAN--
<?php
$dir = sys_get_temp_dir() . '/phl_uncaught_attr';
@unlink($dir . '/uncaught_attr_lib.php');
@rmdir($dir);
