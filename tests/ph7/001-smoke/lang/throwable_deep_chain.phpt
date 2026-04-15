--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Throwable: deep extends chain + explicit implements Throwable
--FILE--
<?php
class DeepA extends Exception {}
class DeepB extends DeepA {}
class DeepC extends DeepB implements Throwable {}
echo (new DeepC() instanceof Throwable) ? "yes\n" : "no\n";
echo (new DeepC() instanceof Exception) ? "yes\n" : "no\n";
try {
    throw new DeepC("d");
} catch (Throwable $t) {
    echo get_class($t), ":", $t->getMessage(), "\n";
}
?>
--EXPECT--
yes
yes
DeepC:d
--CLEAN--
<?php
