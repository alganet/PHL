--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
static:: in a STATIC VARIABLE initializer is late-bound to the runtime class
--DESCRIPTION--
A static variable's initializer is the one initializer php does NOT treat as a
compile-time constant -- it runs inside the CALL, so `static::` there is a real
late-static-binding fetch and php accepts it where it refuses it in every other
initializer position. PHL evaluated it while the frame was still being built, a
step before the class is pushed onto the self stack, so `static::` found no class
at all and said `Class "static" not found`. The initializer still runs ONCE, at
the first call, so the first receiver's binding is what sticks -- for both
engines.
--FILE--
<?php
class SvlsBase {
    const K = 7;
    public function konst()   { static $x = static::K;     return $x; }
    public function name()    { static $y = static::class; return $y; }
    public function selfK()   { static $z = self::K;       return $z; }
}
class SvlsKid extends SvlsBase { const K = 8; }
$kid = new SvlsKid;
var_dump($kid->konst(), $kid->name(), $kid->selfK());
/* The initializer runs ONCE, at the first call, so the first receiver's binding
 * is the one that sticks -- for both engines. */
$base = new SvlsBase;
var_dump($base->konst(), $base->name());
?>
--EXPECT--
int(8)
string(7) "SvlsKid"
int(7)
int(8)
string(7) "SvlsKid"
--CLEAN--
<?php
