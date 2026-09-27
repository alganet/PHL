--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: CachingIterator's untyped $key and int $flags take §10's scalar refusals (PHL half of the twin pair)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* php's CachingIterator declares `$key` untyped and screens it as a STRING in
 * its body, and `$flags` as an int -- so php COERCES null to "" and a
 * fractional float to an int, both with a deprecation §10 does not carry. PHL
 * refuses the same two conversions everywhere else and refuses them here, which
 * is the only difference between the engines on this class. */
$it = new CachingIterator(new ArrayIterator(['x' => 1]),
    CachingIterator::FULL_CACHE | CachingIterator::TOSTRING_USE_KEY);
foreach ($it as $v) {}

/* null reaches every one of the four ArrayAccess members, `$it[] = v` included:
 * php's append passes NULL as the key. */
foreach ([
    'offsetGet' => function ($it) { return $it[null]; },
    'offsetExists' => function ($it) { return isset($it[null]); },
    'offsetSet' => function ($it) { $it[null] = 'v'; return 'written'; },
    'append' => function ($it) { $it[] = 'v'; return 'appended'; },
    'offsetUnset' => function ($it) { unset($it[null]); return 'unset'; },
] as $name => $fn) {
    try { echo $name, ' => ', var_export($fn($it), true), "\n"; }
    catch (Throwable $e) { echo $name, ' => ', get_class($e), ': ', $e->getMessage(), "\n"; }
}
echo 'cache untouched => ', json_encode($it->getCache()), "\n";

/* An INTEGER key is not refused -- it is stringified, exactly as php does. */
echo 'int key => ', var_export(isset($it[0]), true), "\n";

foreach ([1.5, null] as $flags) {
    try { $o = new CachingIterator(new ArrayIterator([1]), $flags); echo 'flags ',
        var_export($flags, true), ' => ', $o->getFlags(), "\n"; }
    catch (Throwable $e) { echo 'flags ', var_export($flags, true), ' => ',
        get_class($e), ': ', $e->getMessage(), "\n"; }
}
?>
--EXPECT--
offsetGet => offsetGet => TypeError: CachingIterator::offsetGet(): Argument #1 ($key) must be of type string, null given
offsetExists => offsetExists => TypeError: CachingIterator::offsetExists(): Argument #1 ($key) must be of type string, null given
offsetSet => offsetSet => TypeError: CachingIterator::offsetSet(): Argument #1 ($key) must be of type string, null given
append => append => TypeError: CachingIterator::offsetSet(): Argument #1 ($key) must be of type string, null given
offsetUnset => offsetUnset => TypeError: CachingIterator::offsetUnset(): Argument #1 ($key) must be of type string, null given
cache untouched => {"x":1}
int key => false
flags 1.5 => TypeError: CachingIterator::__construct(): Argument #2 ($flags) must be of type int, float given
flags NULL => TypeError: CachingIterator::__construct(): Argument #2 ($flags) must be of type int, null given
