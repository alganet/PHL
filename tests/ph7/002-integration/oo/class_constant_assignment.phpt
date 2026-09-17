--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Assignment to class constant attribute
--SKIPIF--
<?php
// php keeps class CONSTANTS and PROPERTIES in separate namespaces, so
// `$t->MY_CONST = 123` writes a dynamic PROPERTY (php allows it here; the class
// constant MY_CONST stays 42). PHL stores both in one table, resolves the member
// write to the CONSTANT, and raises a misleading "Cannot perform assignment on a
// constant class attribute". The right PHL behaviour under §10 would be to reject
// the dynamic property with "Cannot create dynamic property Test::\$MY_CONST" --
// but getting there needs the constant/property namespace split still filed
// §2. Reasoned skip pending that; this test pins PHL's current (wrong) message.
if (function_exists('zend_version')) { echo 'skip const/property namespace collision: PHL resolves the member write to the constant'; }
?>
--FILE--
<?php
class Test {
    const MY_CONST = 42;
}
$t = new Test();
$t->MY_CONST = 123;
?>
--EXPECTF--
Error: Cannot perform assignment on a constant class attribute,PH7 is loading NULL in %s on line %d
--CLEAN--
<?php
unset($t);
