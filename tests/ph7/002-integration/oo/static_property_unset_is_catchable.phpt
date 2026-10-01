--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
unset() on a static property is a catchable Error, not a fatal
--DESCRIPTION--
php refuses `unset(C::$s)` with an ordinary catchable `Error`, and the refusal
is decided BEFORE any member lookup: an undeclared name, a private one, and one
declared on a parent all read the same, and visibility never gets a word in.
Only the class NAME is resolved first, so an unknown class still answers `Class
"X" not found`. The class the message names is whatever the `::` resolved to --
`Child::$b` says Child even when Base declares it, while `parent::$b` says Base
-- and a dynamic name is evaluated (its side effects run) before the refusal.
PHL reported the same wording through the uncaught-fatal path and ABORTED, so
`catch (Throwable)` never ran and everything after the try was dropped with
exit status 0. It is parked on the boundary rail here like the other refusals
on this arm, completing the op as a NULL that carries no slot index, which is
what keeps the trailing generic unset() from clearing the shared class slot.
The name printed is the DISPLAY name: an anonymous class's identity carries
php's NUL-separated `class@anonymous\0file:line$hash`, and formatting that
through a C string truncated the message at the NUL, losing `::$x` entirely.
--FILE--
<?php
class UsBase {
    public static $b = 1;
    protected static $prot = 2;
    private static $priv = 3;
}
class UsChild extends UsBase {
    public static $c = 4;
    public static function inSelf()   { unset(self::$c); }
    public static function inStatic() { unset(static::$c); }
    public static function inParent() { unset(parent::$b); }
    public static function inPriv()   { unset(UsBase::$priv); }
}
trait UsTrait { public static $tv = 9; }
class UsUser { use UsTrait; }

function us($label, $fn) {
    try { $fn(); echo $label, " => NO THROW\n"; }
    catch (Throwable $e) { echo $label, " => ", get_class($e), ": ", $e->getMessage(), "\n"; }
}

echo "== the refusal is decided before any lookup ==\n";
us('own',              function () { unset(UsChild::$c); });
us('inherited',        function () { unset(UsChild::$b); });
us('undeclared',       function () { unset(UsChild::$nope); });
us('protected',        function () { unset(UsChild::$prot); });
us('private outside',  function () { unset(UsBase::$priv); });
us('private inside',   function () { UsChild::inPriv(); });

echo "== the class named is the one the :: resolved to ==\n";
us('self::',           function () { UsChild::inSelf(); });
us('static::',         function () { UsChild::inStatic(); });
us('parent::',         function () { UsChild::inParent(); });
$o = new UsChild;
us('object form',      function () use ($o) { unset($o::$c); });
$n = 'UsChild';
us('class-name var',   function () use ($n) { unset($n::$c); });
us('trait via user',   function () { unset(UsUser::$tv); });
us('trait direct',     function () { unset(UsTrait::$tv); });

echo "== the class name is resolved first ==\n";
us('unknown class',    function () { unset(UsNope::$x); });

echo "== a dynamic name runs before the refusal ==\n";
$p = 'c';
us('dynamic',          function () use ($p) { unset(UsChild::$$p); });
us('dynamic undecl',   function () { $q = 'zz'; unset(UsChild::$$q); });
us('name side effect', function () { $i = 0; unset(UsChild::${'c' . ($i++)}); });

echo "== an anonymous class prints its display name whole ==\n";
$a = new class { public static $x = 1; };
us('anonymous',        function () use ($a) { unset($a::$x); });

echo "== the throw stops the rest of the unset list ==\n";
$keep = 'still here';
us('two in one unset', function () use (&$keep) { unset(UsChild::$c, $keep); });
var_dump($keep);

echo "== and no slot was cleared or de-typed ==\n";
var_dump(UsChild::$c, UsBase::$b, UsUser::$tv, $a::$x);
echo "end\n";
?>
--EXPECT--
== the refusal is decided before any lookup ==
own => Error: Attempt to unset static property UsChild::$c
inherited => Error: Attempt to unset static property UsChild::$b
undeclared => Error: Attempt to unset static property UsChild::$nope
protected => Error: Attempt to unset static property UsChild::$prot
private outside => Error: Attempt to unset static property UsBase::$priv
private inside => Error: Attempt to unset static property UsBase::$priv
== the class named is the one the :: resolved to ==
self:: => Error: Attempt to unset static property UsChild::$c
static:: => Error: Attempt to unset static property UsChild::$c
parent:: => Error: Attempt to unset static property UsBase::$b
object form => Error: Attempt to unset static property UsChild::$c
class-name var => Error: Attempt to unset static property UsChild::$c
trait via user => Error: Attempt to unset static property UsUser::$tv
trait direct => Error: Attempt to unset static property UsTrait::$tv
== the class name is resolved first ==
unknown class => Error: Class "UsNope" not found
== a dynamic name runs before the refusal ==
dynamic => Error: Attempt to unset static property UsChild::$c
dynamic undecl => Error: Attempt to unset static property UsChild::$zz
name side effect => Error: Attempt to unset static property UsChild::$c0
== an anonymous class prints its display name whole ==
anonymous => Error: Attempt to unset static property class@anonymous::$x
== the throw stops the rest of the unset list ==
two in one unset => Error: Attempt to unset static property UsChild::$c
string(10) "still here"
== and no slot was cleared or de-typed ==
int(4)
int(1)
int(9)
int(1)
end
