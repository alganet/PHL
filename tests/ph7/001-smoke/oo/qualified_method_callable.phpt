--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An array callable's method half may name its class: [$obj,'A::m'], ['B','parent::m']
--DESCRIPTION--
php splits the method half at its last `::` and resolves the class half against the
TARGET's class: `self` is the target's class, `parent` its parent, a name is looked up,
and the target must descend from it. The method then runs non-virtually on the target
object, so `[$b,'A::f']` runs A::f even though B overrides it. Every callback door
takes the spelling (is_callable, call_user_func, array_map, usort, Closure::fromCallable).
A missing name on an object reached through another class takes the class-name
catch-all (__callStatic); an inaccessible one still asks the object's __call. php deprecates the spelling; the handler swallows that
notice, which this engine does not raise.
--FILE--
<?php
set_error_handler(fn($n) => $n === E_DEPRECATED);
class QmcZ { function f() { echo "QmcZ::f\n"; } }
class QmcA {
    function f() { echo "QmcA::f ", get_class($this), " ", static::class, "\n"; }
    static function s() { echo "QmcA::s ", static::class, "\n"; }
    private function p() { echo "QmcA::p\n"; }
    function cmp($a, $b) { return $a <=> $b; }
    static function __callStatic($n, $a) { echo "QmcA::__callStatic $n ", static::class, "\n"; }
}
class QmcB extends QmcA {
    function f() { echo "QmcB::f\n"; }
    function cmp($a, $b) { return $b <=> $a; }
    function t() {
        $cases = [[$this, 'QmcA::f'], [$this, 'parent::f'], [$this, 'self::f'],
            [$this, 'PARENT::f'], [$this, 'qmca::F'], [$this, '\QmcA::f'], [$this, 'QmcZ::f'],
            [$this, 'QmcNope::f'], [$this, 'QmcA::s'], [$this, 'QmcA::p'],
            [$this, '::f'], [$this, 'QmcA:::f'], [$this, 'QmcA::f:'],
            ['QmcB', 'QmcA::f'], ['QmcB', 'parent::f'], ['QmcB', 'QmcA::s'], ['QmcA', 'QmcB::f'],
            [new QmcA, 'QmcB::f'], [new QmcA, 'parent::f']];
        foreach ($cases as $c) {
            qmc_run($c);
            try { $cl = Closure::fromCallable($c); $cl(); } catch (Throwable $e) { echo "  fromCallable ", get_class($e), ": ", $e->getMessage(), "\n"; }
        }
        $x = [3, 1, 2]; usort($x, [$this, 'parent::cmp']); echo "usort parent::cmp ", implode(',', $x), "\n";
        $x = [3, 1, 2]; usort($x, [$this, 'self::cmp']); echo "usort self::cmp ", implode(',', $x), "\n";
    }
}
class QmcC extends QmcB {}
function qmc_run($c) {
    $n = null;
    echo json_encode(is_string($c[0]) ? $c : ['obj:' . get_class($c[0]), $c[1]]), " ";
    var_dump(is_callable($c, false, $n));
    echo "  name=$n\n";
    try { call_user_func($c); } catch (Throwable $e) { echo "  ", get_class($e), ": ", $e->getMessage(), "\n"; }
    try { array_map($c, [1]); } catch (Throwable $e) { echo "  array_map ", get_class($e), ": ", $e->getMessage(), "\n"; }
}
(new QmcB)->t();
echo "-- from global scope\n";
$b = new QmcB;
$c = new QmcC;
foreach ([[$b, 'QmcA::f'], [$c, 'parent::f'], [$c, 'QmcA::s'], ['QmcB', 'QmcA::f'],
          ['QmcB', 'QmcA::s'], ['QmcB', 'self::s'], ['QmcB', 'static::s'], [$b, 'QmcA::zz'], [$b, 'QmcA::p'],
          ['QmcB', 'QmcA::zz']] as $cb) {
    qmc_run($cb);
}
restore_error_handler();
?>
--EXPECT--
["obj:QmcB","QmcA::f"] bool(true)
  name=QmcB::QmcA::f
QmcA::f QmcB QmcB
QmcA::f QmcB QmcB
QmcA::f QmcB QmcB
["obj:QmcB","parent::f"] bool(true)
  name=QmcB::parent::f
QmcA::f QmcB QmcB
QmcA::f QmcB QmcB
QmcA::f QmcB QmcB
["obj:QmcB","self::f"] bool(true)
  name=QmcB::self::f
QmcB::f
QmcB::f
QmcB::f
["obj:QmcB","PARENT::f"] bool(true)
  name=QmcB::PARENT::f
QmcA::f QmcB QmcB
QmcA::f QmcB QmcB
QmcA::f QmcB QmcB
["obj:QmcB","qmca::F"] bool(true)
  name=QmcB::qmca::F
QmcA::f QmcB QmcB
QmcA::f QmcB QmcB
QmcA::f QmcB QmcB
["obj:QmcB","\\QmcA::f"] bool(true)
  name=QmcB::\QmcA::f
QmcA::f QmcB QmcB
QmcA::f QmcB QmcB
QmcA::f QmcB QmcB
["obj:QmcB","QmcZ::f"] bool(false)
  name=QmcB::QmcZ::f
  TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, class QmcB is not a subclass of QmcZ
  array_map TypeError: array_map(): Argument #1 ($callback) must be a valid callback or null, class QmcB is not a subclass of QmcZ
  fromCallable TypeError: Failed to create closure from callable: class QmcB is not a subclass of QmcZ
["obj:QmcB","QmcNope::f"] bool(false)
  name=QmcB::QmcNope::f
  TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, class "QmcNope" not found
  array_map TypeError: array_map(): Argument #1 ($callback) must be a valid callback or null, class "QmcNope" not found
  fromCallable TypeError: Failed to create closure from callable: class "QmcNope" not found
["obj:QmcB","QmcA::s"] bool(true)
  name=QmcB::QmcA::s
QmcA::s QmcB
QmcA::s QmcB
QmcA::s QmcB
["obj:QmcB","QmcA::p"] bool(false)
  name=QmcB::QmcA::p
  TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, cannot access private method QmcA::p()
  array_map TypeError: array_map(): Argument #1 ($callback) must be a valid callback or null, cannot access private method QmcA::p()
  fromCallable TypeError: Failed to create closure from callable: cannot access private method QmcA::p()
["obj:QmcB","::f"] bool(false)
  name=QmcB::::f
  TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, invalid function name
  array_map TypeError: array_map(): Argument #1 ($callback) must be a valid callback or null, invalid function name
  fromCallable TypeError: Failed to create closure from callable: invalid function name
["obj:QmcB","QmcA:::f"] bool(false)
  name=QmcB::QmcA:::f
  TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, class "QmcA:" not found
  array_map TypeError: array_map(): Argument #1 ($callback) must be a valid callback or null, class "QmcA:" not found
  fromCallable TypeError: Failed to create closure from callable: class "QmcA:" not found
["obj:QmcB","QmcA::f:"] bool(false)
  name=QmcB::QmcA::f:
  TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, class QmcB does not have a method "QmcA::f:"
  array_map TypeError: array_map(): Argument #1 ($callback) must be a valid callback or null, class QmcB does not have a method "QmcA::f:"
  fromCallable TypeError: Failed to create closure from callable: class QmcB does not have a method "QmcA::f:"
["QmcB","QmcA::f"] bool(false)
  name=QmcB::QmcA::f
  TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, non-static method QmcA::f() cannot be called statically
  array_map TypeError: array_map(): Argument #1 ($callback) must be a valid callback or null, non-static method QmcA::f() cannot be called statically
QmcA::f QmcB QmcB
["QmcB","parent::f"] bool(false)
  name=QmcB::parent::f
  TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, non-static method QmcA::f() cannot be called statically
  array_map TypeError: array_map(): Argument #1 ($callback) must be a valid callback or null, non-static method QmcA::f() cannot be called statically
QmcA::f QmcB QmcB
["QmcB","QmcA::s"] bool(true)
  name=QmcB::QmcA::s
QmcA::s QmcA
QmcA::s QmcA
QmcA::s QmcB
["QmcA","QmcB::f"] bool(false)
  name=QmcA::QmcB::f
  TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, class QmcA is not a subclass of QmcB
  array_map TypeError: array_map(): Argument #1 ($callback) must be a valid callback or null, class QmcA is not a subclass of QmcB
  fromCallable TypeError: Failed to create closure from callable: class QmcA is not a subclass of QmcB
["obj:QmcA","QmcB::f"] bool(false)
  name=QmcA::QmcB::f
  TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, class QmcA is not a subclass of QmcB
  array_map TypeError: array_map(): Argument #1 ($callback) must be a valid callback or null, class QmcA is not a subclass of QmcB
  fromCallable TypeError: Failed to create closure from callable: class QmcA is not a subclass of QmcB
["obj:QmcA","parent::f"] bool(false)
  name=QmcA::parent::f
  TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, cannot access "parent" when current class scope has no parent
  array_map TypeError: array_map(): Argument #1 ($callback) must be a valid callback or null, cannot access "parent" when current class scope has no parent
  fromCallable TypeError: Failed to create closure from callable: cannot access "parent" when current class scope has no parent
usort parent::cmp 1,2,3
usort self::cmp 3,2,1
-- from global scope
["obj:QmcB","QmcA::f"] bool(true)
  name=QmcB::QmcA::f
QmcA::f QmcB QmcB
QmcA::f QmcB QmcB
["obj:QmcC","parent::f"] bool(true)
  name=QmcC::parent::f
QmcB::f
QmcB::f
["obj:QmcC","QmcA::s"] bool(true)
  name=QmcC::QmcA::s
QmcA::s QmcC
QmcA::s QmcC
["QmcB","QmcA::f"] bool(false)
  name=QmcB::QmcA::f
  TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, non-static method QmcA::f() cannot be called statically
  array_map TypeError: array_map(): Argument #1 ($callback) must be a valid callback or null, non-static method QmcA::f() cannot be called statically
["QmcB","QmcA::s"] bool(true)
  name=QmcB::QmcA::s
QmcA::s QmcA
QmcA::s QmcA
["QmcB","self::s"] bool(true)
  name=QmcB::self::s
QmcA::s QmcB
QmcA::s QmcB
["QmcB","static::s"] bool(false)
  name=QmcB::static::s
  TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, cannot access "static" when no class scope is active
  array_map TypeError: array_map(): Argument #1 ($callback) must be a valid callback or null, cannot access "static" when no class scope is active
["obj:QmcB","QmcA::zz"] bool(true)
  name=QmcB::QmcA::zz
QmcA::__callStatic zz QmcB
QmcA::__callStatic zz QmcB
["obj:QmcB","QmcA::p"] bool(false)
  name=QmcB::QmcA::p
  TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, cannot access private method QmcA::p()
  array_map TypeError: array_map(): Argument #1 ($callback) must be a valid callback or null, cannot access private method QmcA::p()
["QmcB","QmcA::zz"] bool(true)
  name=QmcB::QmcA::zz
QmcA::__callStatic zz QmcA
QmcA::__callStatic zz QmcA
