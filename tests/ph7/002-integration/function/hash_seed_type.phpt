--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: a hash() seed that is not an int is refused (PHL half of the twin pair)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* php 8.4 DEPRECATED a seed of any other type "because it is the same as
 * setting the seed to 0", and still hashes with 0 — so `['seed' => $input]`
 * seeds nothing whenever the value arrived as a string, which is how a seed
 * read from a config file or a query string arrives. §10 rejects what php
 * deprecates, so PHL refuses it where it is written. See the zend half. */
foreach (['12', 1.5, [1], null, true] as $bad) {
    try {
        hash('murmur3a', "abc", false, ['seed' => $bad]);
        echo "no refusal\n";
    } catch (TypeError $e) {
        echo $e->getMessage(), "\n";
    }
}
/* an int seed, an unknown key and an algorithm with no seed at all are
 * untouched by the rule — both engines agree on all three */
var_dump(hash('murmur3a', "abc", false, ['seed' => 42]));
var_dump(hash('murmur3a', "abc", false, ['bogus' => 'x']));
var_dump(hash('md5', "abc", false, ['seed' => 'x']));
?>
--EXPECT--
hash(): Argument #4 ($options)["seed"] must be of type int, string given
hash(): Argument #4 ($options)["seed"] must be of type int, float given
hash(): Argument #4 ($options)["seed"] must be of type int, array given
hash(): Argument #4 ($options)["seed"] must be of type int, null given
hash(): Argument #4 ($options)["seed"] must be of type int, bool given
string(8) "4e4f1e68"
string(8) "b3dd93fa"
string(32) "900150983cd24fb0d6963f7d28e17f72"
--CLEAN--
<?php
