--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: unserialize() refuses a dynamic property its write path would refuse (PHL half of the twin pair)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* A payload property that a class neither declares nor opts into is php's
 * E_DEPRECATED `Creation of dynamic property C::$z`, which §10 rejects loudly
 * wherever the engine meets it — so unserialize() raises the same Error its own
 * `$o->z = 1` write path raises, and php raises verbatim for a readonly class.
 * Because it is a throw, the parse is ABANDONED where php's deprecation carries
 * on; the alternative was the silent drop this replaced. */
class UdrP { public $a = 1; }
readonly class UdrR { public int $a; }

foreach ([
    'plain'    => 'O:4:"UdrP":2:{s:1:"a";i:9;s:1:"z";i:7;}',
    'readonly' => 'O:4:"UdrR":2:{s:1:"a";i:9;s:1:"z";i:7;}',
    'abandons' => 'a:2:{i:0;O:4:"UdrP":1:{s:1:"z";i:7;}i:1;s:2:"no";}',
] as $udrLabel => $udrPayload) {
    try { $udrOut = str_replace("\n", '', var_export(unserialize($udrPayload), true)); }
    catch (Throwable $e) { $udrOut = get_class($e) . ': ' . $e->getMessage(); }
    echo $udrLabel, ': ', $udrOut, "\n";
}
?>
--EXPECT--
plain: Error: Cannot create dynamic property UdrP::$z
readonly: Error: Cannot create dynamic property UdrR::$z
abandons: Error: Cannot create dynamic property UdrP::$z
--CLEAN--
<?php
