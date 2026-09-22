--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Class and function use imports are case-insensitive; const imports are not
--FILE--
<?php
namespace A;

#[\Attribute]
class Attr { public function __construct(public string $v = '') {} }
class Cee { const K = 'A\Cee::K'; public static function m() { return 'A\Cee::m'; } }
class Dee extends \Exception {}
interface Eye {}
trait Tee { public function t() { return 'A\Tee::t'; } }
function eff($a = 1) { return "A\\eff($a)"; }
const KAY = 'A\KAY';

namespace B;

use A\Cee;
use A\Dee as Alias;
use A\Eye;
use A\Tee;
use A\Attr;
use A as Ns;
use function A\eff;
use const A\KAY;

// The import alias resolves whatever the spelling at the use site.
echo CEE::K, "\n";
echo cee::m(), "\n";
echo ALIAS::class, "\n";
echo EFF(2), "\n";
echo NS\Cee::K, "\n";           // wrong-case leading segment of an imported namespace
echo ns\cee::K, "\n";

// ...in every position a name can appear.
class Impl implements EYE { use TEE; }
function typed(CEE $c): ?cee { return $c; }
$o = new cEE();
echo typed($o) instanceof CEE ? "instanceof: yes\n" : "instanceof: no\n";
echo (new Impl())->t(), "\n";
try { throw new alias('boom'); } catch (ALIAS $e) { echo 'caught ', $e::class, "\n"; }

#[ATTR('marked')] class Marked {}
echo (new \ReflectionClass(Marked::class))->getAttributes()[0]->getName(), "\n";

// A const import stays BYTE-EXACT: php constants are case-sensitive.
echo KAY, "\n";
try { echo kay; } catch (\Error $e) { echo "const import: case-sensitive\n"; }
?>
--EXPECT--
A\Cee::K
A\Cee::m
A\Dee
A\eff(2)
A\Cee::K
A\Cee::K
instanceof: yes
A\Tee::t
caught A\Dee
A\Attr
A\KAY
const import: case-sensitive
--CLEAN--
<?php
