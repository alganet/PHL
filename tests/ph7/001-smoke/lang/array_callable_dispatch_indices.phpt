--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array-callable DISPATCH reads the integer indices 0 and 1, not insertion order
--DESCRIPTION--
php resolves a [target, method] callable by reading the INTEGER indices 0 and
1. Every dispatch site walked the map's insertion order instead (pFirst,
pFirst->pPrev), so two entries at other keys — ['a'=>'C','b'=>'m'] — were
happily CALLED where php throws "Array callback has to contain indices 0 and
1", and a reversed pair — [1=>'m',0=>'C'] — resolved the method as the class.
The predicate (is_callable) already read the indices after the 27 Jul ship, so
the two answers DISAGREED: is_callable said no and the call ran anyway.
--FILE--
<?php
class AcdiC {
    public static function s($x = 1) { return "s{$x}"; }
    public function m() { return "m"; }
    public static function cmp($a, $b) { return $a <=> $b; }
}

function acdiRun(string $label, callable $fn): void
{
    try {
        $r = $fn();
        echo $label, " => ", var_export($r, true), "\n";
    } catch (Throwable $e) {
        echo $label, " => ", get_class($e), ": ", $e->getMessage(), "\n";
    }
}

/* Wrong keys: two entries, but not at 0 and 1. */
acdiRun('str keys', function () { $cb = ['a' => 'AcdiC', 'b' => 's']; return $cb(); });
acdiRun('obj wrong keys', function () { $cb = ['a' => new AcdiC, 'b' => 'm']; return $cb(); });
acdiRun('sparse', function () { $cb = [0 => 'AcdiC', 5 => 's']; return $cb(); });
acdiRun('literal wrong keys', function () { return ['a' => 'AcdiC', 'b' => 's'](); });

/* Both indices present: index 0 is the target whatever the insertion order. */
acdiRun('reversed', function () { $cb = [1 => 's', 0 => 'AcdiC']; return $cb(); });
acdiRun('numeric string keys', function () { $cb = ["0" => 'AcdiC', "1" => 's']; return $cb(); });
acdiRun('plain', function () { $cb = ['AcdiC', 's']; return $cb(); });

/* The element COUNT is checked first, and keeps its own wording. */
acdiRun('three', function () { $cb = ['AcdiC', 's', 'extra']; return $cb(); });
acdiRun('one', function () { $cb = ['AcdiC']; return $cb(); });

/* The same rule at the callback-ARGUMENT sites, where php names the reason. */
acdiRun('cuf wrong keys', function () { return call_user_func(['a' => 'AcdiC', 'b' => 's']); });
acdiRun('cuf sparse', function () { return call_user_func([0 => 'AcdiC', 5 => 's']); });
acdiRun('cuf reversed', function () { return call_user_func([1 => 's', 0 => 'AcdiC']); });
acdiRun('cuf three', function () { return call_user_func(['AcdiC', 's', 'extra']); });
acdiRun('usort wrong keys', function () {
    $a = [3, 1, 2];
    usort($a, ['a' => 'AcdiC', 'b' => 'cmp']);
    return $a;
});
acdiRun('usort reversed', function () {
    $a = [3, 1, 2];
    usort($a, [1 => 'cmp', 0 => 'AcdiC']);
    return $a;
});

/* The predicate and the dispatch now agree on every shape above. */
foreach ([
    'str keys' => ['a' => 'AcdiC', 'b' => 's'],
    'sparse' => [0 => 'AcdiC', 5 => 's'],
    'reversed' => [1 => 's', 0 => 'AcdiC'],
    'three' => ['AcdiC', 's', 'extra'],
] as $acdi_label => $acdi_cb) {
    echo 'is_callable ', $acdi_label, ' => ', var_export(is_callable($acdi_cb), true), "\n";
}

/* Closure::fromCallable and the first-class-callable syntax normalize a callable
 * VALUE through the same decode. (Their REJECTION wording is php's separate
 * "Failed to create closure from callable: ..." shape — a distinct divergence.) */
acdiRun('fromCallable reversed', function () { return (Closure::fromCallable([1 => 's', 0 => 'AcdiC']))(9); });
acdiRun('fcc reversed', function () { $cb = [1 => 's', 0 => 'AcdiC']; $f = $cb(...); return $f(4); });
echo "end\n";
?>
--EXPECT--
str keys => Error: Array callback has to contain indices 0 and 1
obj wrong keys => Error: Array callback has to contain indices 0 and 1
sparse => Error: Array callback has to contain indices 0 and 1
literal wrong keys => Error: Array callback has to contain indices 0 and 1
reversed => 's1'
numeric string keys => 's1'
plain => 's1'
three => Error: Array callback must have exactly two elements
one => Error: Array callback must have exactly two elements
cuf wrong keys => TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, array callback has to contain indices 0 and 1
cuf sparse => TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, array callback has to contain indices 0 and 1
cuf reversed => 's1'
cuf three => TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, array callback must have exactly two members
usort wrong keys => TypeError: usort(): Argument #2 ($callback) must be a valid callback, array callback has to contain indices 0 and 1
usort reversed => array (
  0 => 1,
  1 => 2,
  2 => 3,
)
is_callable str keys => false
is_callable sparse => false
is_callable reversed => true
is_callable three => false
fromCallable reversed => 's9'
fcc reversed => 's4'
end
--CLEAN--
<?php
unset($acdi_label, $acdi_cb);
