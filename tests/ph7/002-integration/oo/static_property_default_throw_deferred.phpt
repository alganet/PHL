--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A static property default that THROWS is deferred to the first static access
--DESCRIPTION--
php builds a class's static table on first use and evaluates each default
THERE, so `class C { public static $s = UNDEF; }` is silent at the
declaration and raises a catchable `Undefined constant "UNDEF"` at the first
static-property access -- any property of the class, read, write or isset,
through a subclass too, and at instantiation. A class whose bad default is
never read stays silent. Because the evaluation really happens at the
access, a constant define()d after the declaration resolves (with self:: in
the initializer still bound to the DECLARING class, wherever the access was
made from); and a base's broken default is raised before the subclass's own
slots. PHL evaluates
defaults eagerly at mount, so it runs the initializer MUTED there and
re-runs it at the access (PH7_CLASS_ATTR_STATIC_DEFER); before that the
throw was swallowed whole -- the access answered NULL and the process
exited 255 with no diagnostic at all, and a `try` around an include of the
declaration ran its catch at the WRONG time.
--FILE--
<?php
echo "== never read: silent ==\n";
class SdtNever { public static $s = SDT_UNDEF_A; }
echo "declared\n";

echo "== first read raises, catchably, and repeats ==\n";
class SdtRead { public static $s = SDT_UNDEF_B; }
try { $v = SdtRead::$s; } catch (Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { $v = SdtRead::$s; } catch (Error $e) { echo "again: ", get_class($e), ": ", $e->getMessage(), "\n"; }

echo "== write and isset raise too ==\n";
class SdtWrite { public static $s = SDT_UNDEF_C; }
try { SdtWrite::$s = 9; } catch (Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { $b = isset(SdtWrite::$s); } catch (Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }

echo "== any static of the class triggers it; constants and methods do not ==\n";
class SdtOther {
    public static $s = SDT_UNDEF_D;
    public static $ok = 1;
    const K = 7;
    public static function m() { return "m"; }
}
echo SdtOther::K, "\n";
echo SdtOther::m(), "\n";
try { echo SdtOther::$ok, "\n"; } catch (Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }

echo "== instantiation raises before construction ==\n";
class SdtNew {
    public static $s = SDT_UNDEF_E;
    public function __construct() { echo "ctor ran\n"; }
}
try { $o = new SdtNew; } catch (Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }

echo "== the base's slots materialize first ==\n";
class SdtBase { public static $s = SDT_UNDEF_F; }
class SdtChild extends SdtBase { public static $own = 5; }
try { echo SdtChild::$own, "\n"; } catch (Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }

echo "== declaring inside a try catches nothing ==\n";
try {
    class SdtInTry { public static $s = SDT_UNDEF_G; }
    echo "declared\n";
} catch (Error $e) {
    echo "WRONG: caught at the declaration: ", $e->getMessage(), "\n";
}
try { $v = SdtInTry::$s; } catch (Error $e) { echo "at access: ", $e->getMessage(), "\n"; }

echo "== including the declaration catches nothing ==\n";
try {
    include __DIR__ . '/static_property_default_throw_deferred.php';
    echo "include done\n";
} catch (Error $e) {
    echo "WRONG: caught at the include: ", $e->getMessage(), "\n";
}
try { $v = SdtIncluded::$s; } catch (Error $e) { echo "at access: ", $e->getMessage(), "\n"; }

echo "== the initializer runs at the ACCESS, so a later define() resolves ==\n";
class SdtLate { public static $s = SDT_LATE; }
define('SDT_LATE', 42);
var_dump(SdtLate::$s);
var_dump(SdtLate::$s);

echo "== the re-run resolves self:: against the DECLARING class ==\n";
class SdtSelf { const K = 5; public static $t = SDT_LATE2 + self::K; }
class SdtCaller { const K = 99; public static function go() { return SdtSelf::$t; } }
define('SDT_LATE2', 10);
var_dump(SdtCaller::go());

echo "== reading a good sibling still raises the bad slot ==\n";
class SdtPartial { public static $good = 11; public static $bad = SDT_UNDEF_H; }
try { $v = SdtPartial::$good; } catch (Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
echo "end\n";
?>
--EXPECT--
== never read: silent ==
declared
== first read raises, catchably, and repeats ==
Error: Undefined constant "SDT_UNDEF_B"
again: Error: Undefined constant "SDT_UNDEF_B"
== write and isset raise too ==
Error: Undefined constant "SDT_UNDEF_C"
Error: Undefined constant "SDT_UNDEF_C"
== any static of the class triggers it; constants and methods do not ==
7
m
Error: Undefined constant "SDT_UNDEF_D"
== instantiation raises before construction ==
Error: Undefined constant "SDT_UNDEF_E"
== the base's slots materialize first ==
Error: Undefined constant "SDT_UNDEF_F"
== declaring inside a try catches nothing ==
declared
at access: Undefined constant "SDT_UNDEF_G"
== including the declaration catches nothing ==
included
include done
at access: Undefined constant "SDT_UNDEF_INC"
== the initializer runs at the ACCESS, so a later define() resolves ==
int(42)
int(42)
== the re-run resolves self:: against the DECLARING class ==
int(15)
== reading a good sibling still raises the bad slot ==
Error: Undefined constant "SDT_UNDEF_H"
end
--CLEAN--
<?php
unset($v, $b, $o);
