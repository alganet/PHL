--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Named arguments: a duplicate extra into a variadic is refused, and every binding Error is raised at the call site
--FILE--
<?php
function naecs_v(...$r) { var_dump($r); }
function naecs_av($a, ...$r) { var_dump($a, $r); }
function naecs_a($a) {}
class NaecsC {
    function m($a, ...$r) {}
    static function s($a) {}
    function inside() { $this->m(1, q: 1, q: 2); }
}
function naecs_frame(Throwable $e) {
    /* The innermost frame is the CALLER: php raises these before the callee is entered. */
    $t = $e->getTrace()[0];
    return ($t['class'] ?? '') . ($t['type'] ?? '') . preg_replace('/^\{closure:.*/', '{closure}', $t['function']);
}
$cases = [
    'dup extra'          => function () { naecs_v(zz: 2, zz: 3); },
    'dup extra after pos'=> function () { naecs_av(1, zz: 2, zz: 3); },
    'dup extra apart'    => function () { naecs_v(yy: 1, zz: 2, yy: 3); },
    'dup extra spread'   => function () { naecs_v(...['zz' => 1], ...['zz' => 2]); },
    'case differs'       => function () { naecs_v(zz: 1, ZZ: 2); },
    'dup formal'         => function () { naecs_av(a: 1, a: 3); },
    'pos then named'     => function () { naecs_a(1, a: 2); },
    'unknown'            => function () { naecs_a(b: 1); },
    'method dup extra'   => function () { (new NaecsC)->m(1, q: 1, q: 2); },
    'static unknown'     => function () { NaecsC::s(b: 1); },
    'closure unknown'    => function () { (function ($x) {})(y: 1); },
    'from a method'      => function () { (new NaecsC)->inside(); },
];
foreach ($cases as $label => $c) {
    try {
        $c();
        echo "$label: ran\n";
    } catch (Error $e) {
        echo "$label: ", $e->getMessage(), " | ", naecs_frame($e), "\n";
    }
}
?>
--EXPECT--
dup extra: Named parameter $zz overwrites previous argument | {closure}
dup extra after pos: Named parameter $zz overwrites previous argument | {closure}
dup extra apart: Named parameter $yy overwrites previous argument | {closure}
dup extra spread: Named parameter $zz overwrites previous argument | {closure}
array(2) {
  ["zz"]=>
  int(1)
  ["ZZ"]=>
  int(2)
}
case differs: ran
dup formal: Named parameter $a overwrites previous argument | {closure}
pos then named: Named parameter $a overwrites previous argument | {closure}
unknown: Unknown named parameter $b | {closure}
method dup extra: Named parameter $q overwrites previous argument | {closure}
static unknown: Unknown named parameter $b | {closure}
closure unknown: Unknown named parameter $y | {closure}
from a method: Named parameter $q overwrites previous argument | NaecsC->inside
--CLEAN--
<?php
