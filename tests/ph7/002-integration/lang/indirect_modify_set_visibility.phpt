--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An asymmetric set-visibility gates an indirect write too
--DESCRIPTION--
`$o->arr['k'] = v` reaches the property only THROUGH what it holds, and php gates that by the
set-visibility exactly as it gates a plain store -- wording it "indirectly modify", the same
distinction its readonly sentence makes. Only readonly was screened at that point here, so
the write landed in SILENCE on a `private(set)`/`protected(set)` property.
--FILE--
<?php
class ImsvHolder { public private(set) array $t = []; }
$imsv = new ImsvHolder;
$imsv->t['n'] = 'v';
?>
--EXPECTF--
PHP Fatal error:  Uncaught Error: Cannot indirectly modify private(set) property ImsvHolder::$t from global scope in %s:4
Stack trace:
#0 {main}
  thrown in %s on line 4
--CLEAN--
<?php
