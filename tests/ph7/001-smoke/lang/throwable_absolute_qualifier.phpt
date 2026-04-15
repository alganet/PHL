--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Throwable: absolute qualifiers in extends/implements
--FILE--
<?php
class AbsQualA extends \Exception {}
class AbsQualB extends \Error {}
interface AbsQualI extends \Throwable {}
class AbsQualC extends \Error implements AbsQualI {}
echo (new AbsQualA() instanceof Throwable) ? "yes\n" : "no\n";
echo (new AbsQualB() instanceof Throwable) ? "yes\n" : "no\n";
echo (new AbsQualC() instanceof AbsQualI) ? "yes\n" : "no\n";
try {
    throw new AbsQualC("abs");
} catch (\Throwable $t) {
    echo get_class($t), ":", $t->getMessage(), "\n";
}
?>
--EXPECT--
yes
yes
yes
AbsQualC:abs
--CLEAN--
<?php
