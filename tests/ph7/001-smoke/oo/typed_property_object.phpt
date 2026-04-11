--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: bare object type accepts any class instance
--FILE--
<?php
class TpoAlpha { public int $n = 1; }
class TpoBeta { public string $s = "b"; }
class TpoHolder { public object $o; }
$h = new TpoHolder();
$h->o = new TpoAlpha();
echo get_class($h->o), "\n";
$h->o = new TpoBeta();
echo get_class($h->o), "\n";
?>
--EXPECT--
TpoAlpha
TpoBeta
--CLEAN--
<?php
unset($h);
