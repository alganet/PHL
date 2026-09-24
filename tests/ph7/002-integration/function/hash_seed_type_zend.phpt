--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php: a hash() seed that is not an int is a deprecation and hashes with 0 (zend half of the twin pair)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* php deprecates the spelling and carries on with a seed of 0, so every one of
 * these answers the UNSEEDED digest. See the PHL half. */
$plain = hash('murmur3a', "abc");
foreach (['12', 1.5, [1], null, true] as $bad) {
    var_dump(@hash('murmur3a', "abc", false, ['seed' => $bad]) === $plain);
}
var_dump(hash('murmur3a', "abc", false, ['seed' => 42]));
var_dump(hash('murmur3a', "abc", false, ['bogus' => 'x']));
var_dump(hash('md5', "abc", false, ['seed' => 'x']));
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
string(8) "4e4f1e68"
string(8) "b3dd93fa"
string(32) "900150983cd24fb0d6963f7d28e17f72"
--CLEAN--
<?php
