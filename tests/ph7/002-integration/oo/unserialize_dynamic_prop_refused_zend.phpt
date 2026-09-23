--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php: unserialize() creates the dynamic property behind a deprecation (zend half of the twin pair)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--INI--
error_reporting=E_ALL & ~E_DEPRECATED
--FILE--
<?php
/* php's answer to the same three payloads: the plain class creates the property
 * behind `Creation of dynamic property C::$z` (E_DEPRECATED, masked here — §10
 * turns that whole surface into the Error the PHL half pins), a readonly class
 * raises the SAME Error PHL raises everywhere, and the deprecation being only a
 * deprecation is why the containing array parses on. */
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
plain: \UdrP::__set_state(array(   'a' => 9,   'z' => 7,))
readonly: Error: Cannot create dynamic property UdrR::$z
abandons: array (  0 =>   \UdrP::__set_state(array(     'a' => 1,     'z' => 7,  )),  1 => 'no',)
--CLEAN--
<?php
