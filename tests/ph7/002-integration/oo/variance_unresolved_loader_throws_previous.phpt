--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The loader-throw fatal at a declaration carries the exception's previous chain
--DESCRIPTION--
The string form of a chained exception prints the cause first and the
exception after "Next", each with the loader frame called from the
declaration's line.
--INI--
zend.exception_ignore_args=1
--FILE--
<?php
function vuLpLoad($c) {
    echo "load $c\n";
    throw new Exception("no $c", 0, new RuntimeException("cause"));
}
spl_autoload_register('vuLpLoad');
class VuLpParent { function f(): VuLpA {} }
echo "before\n";
class VuLpKid extends VuLpParent { function f(): VuLpB {} }
echo "unreached\n";
?>
--EXPECTF--
before
load VuLpB
%s Fatal error:  During inheritance of VuLpKid, while autoloading VuLpB: Uncaught RuntimeException: cause in %s:4
Stack trace:
#0 %s(9): vuLpLoad()
#1 {main}

Next Exception: no VuLpB in %s:4
Stack trace:
#0 %s(9): vuLpLoad()
#1 {main} in %s on line 9
Stack trace:
#0 {main}
