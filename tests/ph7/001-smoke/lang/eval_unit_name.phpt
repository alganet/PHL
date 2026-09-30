--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An eval()'d chunk is a unit of its own, named after the site that evaluated it
--FILE--
<?php
/* php names an eval()'d compilation unit after the SITE that evaluated it:
 * `<file>(<line>) : eval()'d code`. That name is the unit's IDENTITY, so
 * __FILE__, a diagnostic's location, a Throwable's getFile() and Reflection's
 * getFileName() for anything the chunk declares all read it — and a nested
 * eval names the eval that made it. */
$evSelf = __FILE__;
$evN = function (?string $s) use ($evSelf) {
    return $s === null ? 'NULL' : str_replace($evSelf, 'T', $s);
};

// (1) __FILE__ / __LINE__ inside the chunk.
eval('
echo "1:file=", str_replace($evSelf, "T", __FILE__), "\n";
echo "1:line=", __LINE__, "\n";
echo "1:dir-is-file-dir=", var_export(__DIR__ === dirname($evSelf), true), "\n";
');

// (2) a diagnostic raised inside the chunk names the chunk and its own line.
set_error_handler(function ($no, $str, $file, $line) use ($evN) {
    echo "2:", $evN($file), " @", $line, " ", $str, "\n";
    return true;
});
eval("\n\$evUndefined2 + 1;");
restore_error_handler();

// (3) a Throwable built inside the chunk carries it.
eval('$evT = new Exception("x");');
echo "3:", $evN($evT->getFile()), " @", $evT->getLine(), "\n";

// (4) a class and a function the chunk declares carry it too.
eval('class EvNamedC { public function m() {} }
function ev_named_f() {}');
$evRc = new ReflectionClass('EvNamedC');
echo "4:class=", $evN($evRc->getFileName()), " @", $evRc->getStartLine(), "\n";
$evRm = new ReflectionMethod('EvNamedC', 'm');
echo "4:method=", $evN($evRm->getFileName()), " @", $evRm->getStartLine(), "\n";
$evRf = new ReflectionFunction('ev_named_f');
echo "4:func=", $evN($evRf->getFileName()), " @", $evRf->getStartLine(), "\n";

// (5) a parse error names the chunk, at the offending line.
try { eval("\n\nthis is not php"); }
catch (ParseError $e) { echo "5:", $evN($e->getFile()), " @", $e->getLine(), "\n"; }

// (6) an eval inside a FUNCTION names the function's file, not the caller's.
function ev_named_host() { return eval('return __FILE__;'); }
echo "6:", $evN(ev_named_host()), "\n";

// (7) a nested eval names the eval that made it.
eval('echo "7:", str_replace($evSelf, "T", eval(\'return __FILE__;\')), "\n";');

// (8) one SITE is one name, however many times it runs.
$evSeen = [];
for ($evI = 0; $evI < 3; $evI++) { $evSeen[eval('return __FILE__;')] = 1; }
echo "8:", count($evSeen), "\n";

// (9) a function DECLARED in the file but CALLED from the chunk keeps its own.
function ev_named_outer() { return __FILE__; }
eval('echo "9:", str_replace($evSelf, "T", ev_named_outer()), "\n";');
?>
--EXPECT--
1:file=T(13) : eval()'d code
1:line=3
1:dir-is-file-dir=true
2:T(24) : eval()'d code @2 Undefined variable $evUndefined2
3:T(28) : eval()'d code @1
4:class=T(32) : eval()'d code @1
4:method=T(32) : eval()'d code @1
4:func=T(32) : eval()'d code @2
5:T(42) : eval()'d code @3
6:T(46) : eval()'d code
7:T(50) : eval()'d code(1) : eval()'d code
8:1
9:T
