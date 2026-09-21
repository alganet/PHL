--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Whole-real default into an int property: php throws (zend half of the twin pair)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--FILE--
<?php
const TpwFloat = 2.0;
class TpwInt   { public int $p = TpwFloat; }
class TpwUnion { public int|string $p = TpwFloat; }
try { new TpwInt; }   catch (TypeError $e) { echo $e->getMessage(), "\n"; }
try { new TpwUnion; } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
echo "done\n";
?>
--EXPECT--
Cannot assign float to property TpwInt::$p of type int
Cannot assign float to property TpwUnion::$p of type string|int
done
--CLEAN--
<?php
