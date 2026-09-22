--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Direct dispatch of a callable VALUE names php's Errors instead of answering NULL
--DESCRIPTION--
The "Class::method" STRING form went straight to the shared dispatcher, whose
contract is to answer SXRET_OK with a NULL result for a pair it cannot resolve —
so `$cb = 'C::nosuch'; $cb();` evaluated to NULL with NO diagnostic, where php
throws. The ARRAY form of the same call already threw; both now go through one
resolve check. Two more php details came with it: the string is split on the
LAST "::" (so 'C::s::x' names the class "C::s"), and the array form names a
member of the wrong SHAPE ("First array member is not a valid class name or
object" / "Second array member is not a valid method") before resolving
anything.
--FILE--
<?php
class CvdeC {
    public static function s($x = 1) { return "s{$x}"; }
    public function m() { return 'm'; }
    private static function p() { return 'p'; }
}

function cvdeRun(string $label, callable $fn): void
{
    try {
        $out = var_export($fn(), true);
    } catch (Throwable $e) {
        $out = get_class($e) . ': ' . $e->getMessage();
    }
    echo $label, ' => ', $out, "\n";
}

/* The string form: every failure is a catchable Error, never a silent NULL. */
cvdeRun('str ok', function () { $cb = 'CvdeC::s'; return $cb(); });
cvdeRun('str ok args', function () { $cb = 'CvdeC::s'; return $cb(7); });
cvdeRun('str anchored', function () { $cb = '\CvdeC::s'; return $cb(); });
cvdeRun('str undef method', function () { $cb = 'CvdeC::nosuch'; return $cb(); });
cvdeRun('str undef class', function () { $cb = 'NoSuchCvde::m'; return $cb(); });
cvdeRun('str private', function () { $cb = 'CvdeC::p'; return $cb(); });
cvdeRun('str empty method', function () { $cb = 'CvdeC::'; return $cb(); });
cvdeRun('str empty class', function () { $cb = '::s'; return $cb(); });
cvdeRun('str bare colons', function () { $cb = '::'; return $cb(); });
cvdeRun('str double scope', function () { $cb = 'CvdeC::s::x'; return $cb(); });
cvdeRun('str plain undef fn', function () { $cb = 'nosuchCvdeFunction'; return $cb(); });

/* The array form: the member SHAPE decides before the class is looked up. */
cvdeRun('arr int target', function () { $cb = [5, 's']; return $cb(); });
cvdeRun('arr array target', function () { $cb = [[1], 's']; return $cb(); });
cvdeRun('arr bool target', function () { $cb = [true, 's']; return $cb(); });
cvdeRun('arr int method', function () { $cb = ['CvdeC', 5]; return $cb(); });
cvdeRun('arr null method', function () { $cb = ['CvdeC', null]; return $cb(); });
cvdeRun('arr obj int method', function () { $cb = [new CvdeC, 5]; return $cb(); });
cvdeRun('arr unknown+int', function () { $cb = ['NoSuchCvde', 5]; return $cb(); });
cvdeRun('arr empty method', function () { $cb = ['CvdeC', '']; return $cb(); });
cvdeRun('arr empty class', function () { $cb = ['', 's']; return $cb(); });

/* Caught mid-expression, the rest of the script keeps running. */
try {
    $cvde_r = 1 + (function () { $cb = 'CvdeC::nosuch'; return $cb(); })();
} catch (Error $e) {
    echo 'caught: ', $e->getMessage(), "\n";
}
echo 'still here', "\n";
echo 'after: ', var_export((function () { $cb = 'CvdeC::s'; return $cb(3); })(), true), "\n";
echo "end\n";
?>
--EXPECT--
str ok => 's1'
str ok args => 's7'
str anchored => 's1'
str undef method => Error: Call to undefined method CvdeC::nosuch()
str undef class => Error: Class "NoSuchCvde" not found
str private => Error: Call to private method CvdeC::p() from global scope
str empty method => Error: Call to undefined method CvdeC::()
str empty class => Error: Class "" not found
str bare colons => Error: Class "" not found
str double scope => Error: Class "CvdeC::s" not found
str plain undef fn => Error: Call to undefined function nosuchCvdeFunction()
arr int target => Error: First array member is not a valid class name or object
arr array target => Error: First array member is not a valid class name or object
arr bool target => Error: First array member is not a valid class name or object
arr int method => Error: Second array member is not a valid method
arr null method => Error: Second array member is not a valid method
arr obj int method => Error: Second array member is not a valid method
arr unknown+int => Error: Second array member is not a valid method
arr empty method => Error: Call to undefined method CvdeC::()
arr empty class => Error: Class "" not found
caught: Call to undefined method CvdeC::nosuch()
still here
after: 's3'
end
--CLEAN--
<?php
unset($cvde_r, $e);
