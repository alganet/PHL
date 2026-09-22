--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A lazily-evaluated class initializer reports the ACCESS site, not the declaration
--DESCRIPTION--
php evaluates a class constant's expression, and a static property default
whose evaluation was deferred, AT THE ACCESS -- so the Throwable it raises
carries the accessing line and file, which is also what the uncaught report
prints. PHL runs the initializer's own bytecode there, whose instructions carry
the DECLARATION's line, so `getLine()` pointed at the class body and an uncaught
report pointed a reader at a line that merely declares a constant. Two sites
keep their own rule: an enum CASE reports the case's line in both engines
(php stamps it at the case, and PHL already matched), and a typed static's
deferred TypeError is raised at the access site directly, never through an
initializer. The access line covers the initializer's OWN bytecode only:
whatever it manages to call -- an autoloader firing on a class-constant fetch,
or a NESTED initializer -- reports its own lines, which is php's rule too.
--FILE--
<?php
const LciStr = "abc";
class LciStatic { public static $s = LCI_UNDEF; }
class LciConst { const K = LCI_UNDEF2; }
class LciTypedStatic { public static int $s = LciStr; }
enum LciEnum: int { case A = LCI_UNDEF3; }
class LciTypedConst { const int K = LCI_UNDEF4; }

function lci_report($what, $fn) {
    try {
        $fn();
    } catch (Throwable $e) {
        echo $what, ": ", get_class($e), " at line ", $e->getLine(),
             ", file ", basename($e->getFile()), "\n";
    }
}
lci_report('static default', function () { return LciStatic::$s; });
lci_report('class constant', function () { return LciConst::K; });
lci_report('typed static  ', function () { return LciTypedStatic::$s; });
lci_report('enum case     ', function () { return LciEnum::A; });
lci_report('typed constant', function () { return LciTypedConst::K; });

echo "-> a NESTED evaluation reports the fetch inside the initializer\n";
class LciInner {
    const K =
        LCI_UNDEF5;
}
class LciOuter {
    const K =
        LciInner::K;
}
try { LciOuter::K; } catch (Throwable $e) { echo "line ", $e->getLine(), "\n"; }

echo "-> an autoloader called from an initializer keeps its OWN lines\n";
spl_autoload_register(function ($n) {
    if ($n === 'LciAuto') { throw new RuntimeException("no such class"); }
});
class LciHost { const K = LciAuto::X; }
try { LciHost::K; } catch (Throwable $e) { echo get_class($e), " at line ", $e->getLine(), "\n"; }

echo "-> the same access from two different lines\n";
try { LciStatic::$s; } catch (Throwable $e) { echo "line ", $e->getLine(), "\n"; }
try { LciStatic::$s; } catch (Throwable $e) { echo "line ", $e->getLine(), "\n"; }
echo "end\n";
?>
--EXPECTF--
static default: Error at line 17, file %s
class constant: Error at line 18, file %s
typed static  : TypeError at line 19, file %s
enum case     : Error at line 6, file %s
typed constant: Error at line 21, file %s
-> a NESTED evaluation reports the fetch inside the initializer
line 30
-> an autoloader called from an initializer keeps its OWN lines
RuntimeException at line 36
-> the same access from two different lines
line 42
line 43
end
--CLEAN--
<?php
