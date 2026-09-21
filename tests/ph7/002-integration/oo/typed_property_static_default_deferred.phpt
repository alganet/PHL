--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed static property defaults are validated lazily at first static access or instantiation
--DESCRIPTION--
php evaluates static-property defaults lazily, so a type-invalid default
(`public static int $s = FLS` with FLS = "abc") is silent until the class's
static table materializes: ANY static-property access (read, write, even
isset — any property of the class, including through a subclass) and ANY
instantiation throws the catchable "Cannot assign string to property C::$s
of type int" TypeError, repeatedly; constants and static method calls do
not trigger it, and a never-touched class stays silent. PHL evaluates the
default eagerly at mount, so it defers the failure instead (slot flag +
class hint, VmThrowDeferredStaticType). A VALID int default into a `float`
static now also widens at mount (php shows float(1)).
--FILE--
<?php
const SdStr = "abc";
const SdNull = null;

echo "== read throws, repeatedly ==\n";
class SdRead { public static int $s = SdStr; }
try { $v = SdRead::$s; } catch (Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { $v = SdRead::$s; } catch (Error $e) { echo "again: ", get_class($e), "\n"; }

echo "== write throws too ==\n";
class SdWrite { public static int $s = SdStr; }
try { SdWrite::$s = 9; } catch (Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }

echo "== isset throws too ==\n";
class SdIsset { public static int $s = SdStr; }
try { $b = isset(SdIsset::$s); } catch (Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }

echo "== any static of the class triggers it ==\n";
class SdOther { public static int $s = SdStr; public static $ok = 1; }
try { echo SdOther::$ok, "\n"; } catch (Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }

echo "== constants and static methods do NOT ==\n";
class SdConst { public static int $s = SdStr; const K = 7; public static function m() { return "m"; } }
echo SdConst::K, "\n";
echo SdConst::m(), "\n";

echo "== instantiation triggers it, before construction ==\n";
class SdNew { public static int $s = SdStr; public function __construct() { echo "ctor ran\n"; } }
try { $o = new SdNew; } catch (Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }

echo "== subclass access names the declaring class ==\n";
class SdBase { public static int $s = SdStr; }
class SdChild extends SdBase { public static $own = 5; }
try { echo SdChild::$own, "\n"; } catch (Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }

echo "== null default into non-nullable ==\n";
class SdNullDef { public static int $s = SdNull; }
try { $v = SdNullDef::$s; } catch (Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }

echo "== valid defaults: widening + nullable + union ==\n";
class SdOk {
    public static float $f = 1;
    public static ?int $n = SdNull;
    public static int|string $u = "x";
}
var_dump(SdOk::$f, SdOk::$n, SdOk::$u);

echo "== never accessed: silent ==\n";
class SdNever { public static int $s = SdStr; }
echo "end\n";
?>
--EXPECT--
== read throws, repeatedly ==
TypeError: Cannot assign string to property SdRead::$s of type int
again: TypeError
== write throws too ==
TypeError: Cannot assign string to property SdWrite::$s of type int
== isset throws too ==
TypeError: Cannot assign string to property SdIsset::$s of type int
== any static of the class triggers it ==
TypeError: Cannot assign string to property SdOther::$s of type int
== constants and static methods do NOT ==
7
m
== instantiation triggers it, before construction ==
TypeError: Cannot assign string to property SdNew::$s of type int
== subclass access names the declaring class ==
TypeError: Cannot assign string to property SdBase::$s of type int
== null default into non-nullable ==
TypeError: Cannot assign null to property SdNullDef::$s of type int
== valid defaults: widening + nullable + union ==
float(1)
NULL
string(1) "x"
== never accessed: silent ==
end
--CLEAN--
<?php
unset($v, $b, $o);
