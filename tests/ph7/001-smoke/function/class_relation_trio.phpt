--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
class_parents/class_implements/class_uses answer for every class-like name
--DESCRIPTION--
All three were prelude wrappers gated on class_exists(), and every part of that
was wrong. An interface, a trait and an enum answered FALSE where php answers a
list; class_uses could not reach the trait table at all and returned the empty
set for EVERY class, so the one function whose whole job is naming traits named
none; a name nothing declares was answered with a bare false where php raises an
E_WARNING (with two spellings -- whether it was allowed to autoload decides
which); and an argument that is neither an object nor a string was resolved
through a string cast instead of php's TypeError.
--FILE--
<?php
interface CrtI {}
interface CrtJ extends CrtI {}
trait CrtT { public function t() { return 't'; } }
trait CrtU { use CrtT; }
abstract class CrtBase implements CrtJ { use CrtU; }
class CrtMid extends CrtBase { use CrtT; }
final class CrtLeaf extends CrtMid {}
enum CrtE: string { case X = 'x'; }

set_error_handler(function ($no, $str) { echo '  ERR(', $no, '): ', $str, "\n"; return true; });
$crtShow = function ($label, $fn) {
    try { $out = $fn(); }
    catch (Throwable $e) { echo $label, ' => ', get_class($e), ': ', $e->getMessage(), "\n"; return; }
    echo $label, ' => ', is_array($out) ? '[' . implode(',', array_keys($out)) . ']'
                                        : var_export($out, true), "\n";
};

foreach (['class_parents', 'class_implements', 'class_uses'] as $crtFn) {
    foreach (['CrtLeaf', 'CrtMid', 'CrtBase', 'CrtI', 'CrtJ', 'CrtT', 'CrtU', 'CrtE',
              'ArrayObject', 'crtmid', '\CrtMid'] as $crtName) {
        $crtShow("$crtFn('$crtName')", fn() => $crtFn($crtName));
    }
    $crtShow("$crtFn(new CrtLeaf)", fn() => $crtFn(new CrtLeaf()));
    $crtShow("$crtFn(CrtE::X)", fn() => $crtFn(CrtE::X));
    /* The two spellings of "no such class", and php's own TypeError. */
    $crtShow("$crtFn('CrtNope')", fn() => $crtFn('CrtNope'));
    $crtShow("$crtFn('CrtNope', false)", fn() => $crtFn('CrtNope', false));
    $crtShow("$crtFn('')", fn() => $crtFn(''));
    $crtShow("$crtFn(1)", fn() => $crtFn(1));
    $crtShow("$crtFn(null)", fn() => $crtFn(null));
    $crtShow("$crtFn([])", fn() => $crtFn([]));
    $crtShow("$crtFn(true)", fn() => $crtFn(true));
}
restore_error_handler();

/* Reflection reports NO type on $object_or_class, which is what php reports. */
$crtParam = (new ReflectionFunction('class_parents'))->getParameters();
var_dump($crtParam[0]->hasType(), (string)$crtParam[1]->getType());
--EXPECT--
class_parents('CrtLeaf') => [CrtMid,CrtBase]
class_parents('CrtMid') => [CrtBase]
class_parents('CrtBase') => []
class_parents('CrtI') => []
class_parents('CrtJ') => []
class_parents('CrtT') => []
class_parents('CrtU') => []
class_parents('CrtE') => []
class_parents('ArrayObject') => []
class_parents('crtmid') => [CrtBase]
class_parents('\CrtMid') => [CrtBase]
class_parents(new CrtLeaf) => [CrtMid,CrtBase]
class_parents(CrtE::X) => []
  ERR(2): class_parents(): Class CrtNope does not exist and could not be loaded
class_parents('CrtNope') => false
  ERR(2): class_parents(): Class CrtNope does not exist
class_parents('CrtNope', false) => false
  ERR(2): class_parents(): Class  does not exist and could not be loaded
class_parents('') => false
class_parents(1) => TypeError: class_parents(): Argument #1 ($object_or_class) must be of type object|string, int given
class_parents(null) => TypeError: class_parents(): Argument #1 ($object_or_class) must be of type object|string, null given
class_parents([]) => TypeError: class_parents(): Argument #1 ($object_or_class) must be of type object|string, array given
class_parents(true) => TypeError: class_parents(): Argument #1 ($object_or_class) must be of type object|string, true given
class_implements('CrtLeaf') => [CrtJ,CrtI]
class_implements('CrtMid') => [CrtI,CrtJ]
class_implements('CrtBase') => [CrtJ,CrtI]
class_implements('CrtI') => []
class_implements('CrtJ') => [CrtI]
class_implements('CrtT') => []
class_implements('CrtU') => []
class_implements('CrtE') => [UnitEnum,BackedEnum]
class_implements('ArrayObject') => [IteratorAggregate,Traversable,ArrayAccess,Serializable,Countable]
class_implements('crtmid') => [CrtI,CrtJ]
class_implements('\CrtMid') => [CrtI,CrtJ]
class_implements(new CrtLeaf) => [CrtJ,CrtI]
class_implements(CrtE::X) => [UnitEnum,BackedEnum]
  ERR(2): class_implements(): Class CrtNope does not exist and could not be loaded
class_implements('CrtNope') => false
  ERR(2): class_implements(): Class CrtNope does not exist
class_implements('CrtNope', false) => false
  ERR(2): class_implements(): Class  does not exist and could not be loaded
class_implements('') => false
class_implements(1) => TypeError: class_implements(): Argument #1 ($object_or_class) must be of type object|string, int given
class_implements(null) => TypeError: class_implements(): Argument #1 ($object_or_class) must be of type object|string, null given
class_implements([]) => TypeError: class_implements(): Argument #1 ($object_or_class) must be of type object|string, array given
class_implements(true) => TypeError: class_implements(): Argument #1 ($object_or_class) must be of type object|string, true given
class_uses('CrtLeaf') => []
class_uses('CrtMid') => [CrtT]
class_uses('CrtBase') => [CrtU]
class_uses('CrtI') => []
class_uses('CrtJ') => []
class_uses('CrtT') => []
class_uses('CrtU') => [CrtT]
class_uses('CrtE') => []
class_uses('ArrayObject') => []
class_uses('crtmid') => [CrtT]
class_uses('\CrtMid') => [CrtT]
class_uses(new CrtLeaf) => []
class_uses(CrtE::X) => []
  ERR(2): class_uses(): Class CrtNope does not exist and could not be loaded
class_uses('CrtNope') => false
  ERR(2): class_uses(): Class CrtNope does not exist
class_uses('CrtNope', false) => false
  ERR(2): class_uses(): Class  does not exist and could not be loaded
class_uses('') => false
class_uses(1) => TypeError: class_uses(): Argument #1 ($object_or_class) must be of type object|string, int given
class_uses(null) => TypeError: class_uses(): Argument #1 ($object_or_class) must be of type object|string, null given
class_uses([]) => TypeError: class_uses(): Argument #1 ($object_or_class) must be of type object|string, array given
class_uses(true) => TypeError: class_uses(): Argument #1 ($object_or_class) must be of type object|string, true given
bool(false)
string(4) "bool"
