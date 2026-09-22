--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
POLICY DIVERGENCE §10: a LOSSY float array_key_exists() key deprecates and truncates (php half)
--DESCRIPTION--
The php half of array_key_exists_lossy_float_key.phpt: php emits
`Implicit conversion from float 5.7 to int loses precision` (E_DEPRECATED) and looks up the
truncated key. PHL has no engine deprecation sites and rejects the lossy float instead (§10).
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip php half of the twin pair; PHL's behaviour is in the non-_zend member";
}
?>
--FILE--
<?php
set_error_handler(function ($no, $str) { echo "  [$no] $str\n"; return true; });
$a = [5 => 'five'];
try { var_dump(array_key_exists(5.7, $a)); } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { var_dump(key_exists(-5.7, $a)); } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { var_dump($a[5.7]); } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
var_dump(array_key_exists(5.0, $a));
?>
--EXPECT--
  [8192] Implicit conversion from float 5.7 to int loses precision
bool(true)
  [8192] Implicit conversion from float -5.7 to int loses precision
bool(false)
  [8192] Implicit conversion from float 5.7 to int loses precision
string(4) "five"
bool(true)
