--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
self/parent in a parameter default resolve against the declaring class
--FILE--
<?php
/* `self` in a parameter default is the DECLARING class, not the one the call was
 * made through -- the same rule a member initializer follows. */
class PdsBase { const K = 1; public function m(int $x = self::K) { return $x; } }
class PdsSub extends PdsBase { const K = 2; }
trait PdsT { public function t(int $x = self::K) { return $x; } }
class PdsUse { use PdsT; const K = 3; }
class PdsUseSub extends PdsUse { const K = 4; }
trait PdsP { public function p(int $x = parent::PK) { return $x; } }
class PdsPBase { const PK = 5; }
class PdsPUse extends PdsPBase { use PdsP; }
var_dump((new PdsBase)->m(), (new PdsSub)->m(),
         (new PdsUse)->t(), (new PdsUseSub)->t(),
         (new PdsPUse)->p(), (new PdsBase)->m(9));
?>
--EXPECT--
int(1)
int(1)
int(3)
int(3)
int(5)
int(9)
