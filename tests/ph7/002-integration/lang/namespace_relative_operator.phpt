--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The namespace\ NAME OPERATOR (php 5.3): namespace\X is the current namespace, absolute
--FILE--
<?php
namespace Other;

class Cee { const K = 'Other\Cee'; }
interface Iface {}
function eff() { return 'Other\eff'; }
const KAY = 'Other\KAY';

namespace B;

class Cee { const K = 'B\Cee'; public static $s = 'B::$s'; public static function m() { return 'B::m'; } }
interface Iface {}
class E1 extends \Exception {}
class E2 extends \Exception {}
function eff() { return 'B\eff'; }
const KAY = 'B\KAY';

echo namespace\Cee::K, '|', namespace\eff(), '|', namespace\KAY, "\n";
echo namespace\Cee::class, '|', namespace\Cee::$s, '|', namespace\Cee::m(), "\n";

// new / instanceof / extends / implements / catch / type hints
class Sub extends namespace\Cee implements namespace\Iface {}
$o = new namespace\Sub;
var_dump($o instanceof namespace\Cee, $o instanceof namespace\Iface, $o instanceof \Other\Cee);

try { throw new E2('boom'); } catch (namespace\E1 | namespace\E2 $e) { echo 'caught ', get_class($e), "\n"; }

function typed(namespace\Cee $c): namespace\Cee|namespace\Iface { return $c; }
echo get_class(typed(new namespace\Cee)), "\n";

// The keyword folds like every other keyword, and a STATEMENT may start with it.
echo NAMESPACE\Cee::K, "\n";
namespace\Cee::m();

// Multi-segment relative names, and the operator inside a nested scope.
namespace B\Deep;

class Leaf { const K = 'B\Deep\Leaf'; }

namespace B;

echo namespace\Deep\Leaf::K, "\n";
$fn = function () { return namespace\Cee::K; };
echo $fn(), '|', (fn() => namespace\eff())(), "\n";
$anon = new class extends namespace\Cee {};   // deferred chunk
echo get_parent_class($anon), "\n";

// No fallback of any kind: a namespace\ name is fully qualified.
try { new namespace\Exception; } catch (\Error $e) { echo $e->getMessage(), "\n"; }
try { namespace\strlen('x'); } catch (\Error $e) { echo $e->getMessage(), "\n"; }
try { echo namespace\PHP_EOL; } catch (\Error $e) { echo $e->getMessage(), "\n"; }

namespace C;

// An import NEVER applies to a namespace\ name: the short name is the import,
// the relative one is C\... whether or not anything declares it.
use Other\Cee;
use function Other\eff;
use const Other\KAY;

echo Cee::K, '|', eff(), '|', KAY, "\n";
try { new namespace\Cee; } catch (\Error $e) { echo $e->getMessage(), "\n"; }
try { namespace\eff(); } catch (\Error $e) { echo $e->getMessage(), "\n"; }
try { echo namespace\KAY; } catch (\Error $e) { echo $e->getMessage(), "\n"; }
var_dump(new Cee instanceof namespace\Cee);
?>
--EXPECT--
B\Cee|B\eff|B\KAY
B\Cee|B::$s|B::m
bool(true)
bool(true)
bool(false)
caught B\E2
B\Cee
B\Cee
B\Deep\Leaf
B\Cee|B\eff
B\Cee
Class "B\Exception" not found
Call to undefined function B\strlen()
Undefined constant "B\PHP_EOL"
Other\Cee|Other\eff|Other\KAY
Class "C\Cee" not found
Call to undefined function C\eff()
Undefined constant "C\KAY"
bool(false)
--CLEAN--
<?php
