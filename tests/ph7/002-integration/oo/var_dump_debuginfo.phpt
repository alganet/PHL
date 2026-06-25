--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
var_dump and print_r consult __debugInfo(); var_export and foreach use real props
--DESCRIPTION--
PHL-only on the dump byte format. Semantics match PHP: var_dump/print_r show the
__debugInfo() array (real "secret" hidden), while var_export and foreach see the
real properties.
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
class D {
    public $secret = 42;
    public function __debugInfo() { return ['shown' => 'yes', 'n' => 7]; }
}
$o = new D;
var_dump($o);
print_r($o);
echo "\n";
var_export($o);
echo "\n";
foreach ($o as $k => $v) { echo "$k=$v\n"; }
?>
--EXPECT--
object(D)#1 (2) {
 [shown] =>
  string(3) "yes"
 [n] =>
  int(7)
 }
Object(D) {
 [shown] =>
  yes
 [n] =>
  7
 }

\D::__set_state(array(
   'secret' => 42,
))
secret=42
--CLEAN--
<?php
