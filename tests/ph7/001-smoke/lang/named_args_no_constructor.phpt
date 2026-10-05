--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Named arguments: a `new` on a class with no constructor refuses the first name at the call site and builds no object
--FILE--
<?php
class NancPlain {}
class NancDtor { function __destruct() { echo "destructed\n"; } }
function nanc_new($c) { return new $c(k: 1); }
$nancCases = [
    'literal'       => fn() => new NancPlain(a: 1),
    'stdClass'      => fn() => new stdClass(a: 1),
    'after pos'     => fn() => new NancPlain(1, b: 2),
    'spread key'    => fn() => new NancPlain(...['c' => 1]),
    'spread mixed'  => fn() => new NancPlain(...[1, 'd' => 2]),
    'iterator key'  => fn() => new NancPlain(...new ArrayIterator(['e' => 1])),
    'dynamic name'  => fn() => nanc_new('NancPlain'),
    'anonymous'     => fn() => new class(f: 1) {},
    'no destructor' => fn() => new NancDtor(g: 1),
    'positional'    => fn() => new NancPlain(1, 2),
    'empty spread'  => fn() => new NancPlain(...[]),
];
foreach ($nancCases as $label => $f) {
    try {
        $o = $f();
        echo $label, ": built ", get_class($o) === 'NancPlain' ? 'NancPlain' : get_class($o), "\n";
    } catch (Error $e) {
        echo $label, ": ", get_class($e), ": ", $e->getMessage(), " @", $e->getLine(), "\n";
    }
}
/* Caught inside the expression that was building an array. */
$nancIn = function () {
    try { return [1, new NancPlain(h: 1)]; } finally { echo "finally ran\n"; }
};
try { $nancIn(); } catch (Error $e) { echo $e->getMessage(), "\n"; }
try { nanc_new('NancPlain'); } catch (Error $e) {
    echo $e->getTrace()[0]['function'], " line ", $e->getLine(), "\n";
}
echo "end\n";
?>
--EXPECT--
literal: Error: Unknown named parameter $a @6
stdClass: Error: Unknown named parameter $a @7
after pos: Error: Unknown named parameter $b @8
spread key: Error: Unknown named parameter $c @9
spread mixed: Error: Unknown named parameter $d @10
iterator key: Error: Unknown named parameter $e @11
dynamic name: Error: Unknown named parameter $k @4
anonymous: Error: Unknown named parameter $f @13
no destructor: Error: Unknown named parameter $g @14
positional: built NancPlain
empty spread: built NancPlain
finally ran
Unknown named parameter $h
nanc_new line 4
end
