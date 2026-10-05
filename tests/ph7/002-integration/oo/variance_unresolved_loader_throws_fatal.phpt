--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A loader that throws while a declaration settles its variance pairs is fatal, not caught
--DESCRIPTION--
The class is half-linked when the autoload runs, so php turns the loader's
exception into a fatal at the declaration: neither the catch nor the finally
around it runs, the trace names the declaration's first line, and
error_get_last() reports it as E_ERROR there.
--INI--
zend.exception_ignore_args=1
--FILE--
<?php
register_shutdown_function(function () {
    $e = error_get_last();
    echo "shutdown: type ", $e['type'], ", line ", $e['line'], "\n";
});
function vuLtLoad($c) {
    echo "load $c\n";
    throw new LogicException("no $c");
}
spl_autoload_register('vuLtLoad');
class VuLtParent { function f(VuLtA $a) {} }
try {
    class VuLtKid
        extends VuLtParent
    {
        function f(VuLtB $a) {}
    }
    echo "unreached\n";
} catch (Throwable $e) {
    echo "caught\n";
} finally {
    echo "finally\n";
}
echo "unreached\n";
?>
--EXPECTF--
load VuLtA
%s Fatal error:  During inheritance of VuLtKid, while autoloading VuLtA: Uncaught LogicException: no VuLtA in %s:8
Stack trace:
#0 %s(13): vuLtLoad()
#1 {main} in %s on line 13
Stack trace:
#0 {main}
shutdown: type 1, line 13
