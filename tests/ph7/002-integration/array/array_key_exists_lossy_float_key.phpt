--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
POLICY DIVERGENCE §10: a LOSSY float array_key_exists() key is PHL's TypeError (PHL half)
--DESCRIPTION--
php DEPRECATES a lossy float used as an array offset and truncates it, so
`array_key_exists(5.7, [5 => 'x'])` is true after `Implicit conversion from float 5.7 to int
loses precision`. PHL targets php's NON-deprecated surface (§10) and rejects the lossy float
everywhere -- and now with the same message the subscript gives, because the builtin and
`$a[5.7]` share one rule (PH7_VmArrayKeyArg). A WHOLE float is a key in both engines; only the
lossy one diverges. php's half is the `_zend` twin.
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL half of the twin pair; php's behaviour is in the _zend twin";
}
?>
--FILE--
<?php
$a = [5 => 'five'];
try { var_dump(array_key_exists(5.7, $a)); } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { var_dump(key_exists(-5.7, $a)); } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { var_dump($a[5.7]); } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
var_dump(array_key_exists(5.0, $a));
?>
--EXPECT--
TypeError: Cannot access offset of type float on array
TypeError: Cannot access offset of type float on array
TypeError: Cannot access offset of type float on array
bool(true)
