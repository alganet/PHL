--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php: CachingIterator's untyped $key and int $flags COERCE null and a fractional float (zend half of the twin pair)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* The coercions themselves: null becomes the "" key and 1.5 becomes the int 1,
 * each with a deprecation §10 keeps off this corpus. */
error_reporting(E_ALL & ~E_DEPRECATED);
$it = new CachingIterator(new ArrayIterator(['x' => 1]),
    CachingIterator::FULL_CACHE | CachingIterator::TOSTRING_USE_KEY);
foreach ($it as $v) {}
foreach ([
    'offsetGet' => function ($it) { return @$it[null]; },
    'offsetExists' => function ($it) { return isset($it[null]); },
    'offsetSet' => function ($it) { $it[null] = 'v'; return 'written'; },
    'append' => function ($it) { $it[] = 'v'; return 'appended'; },
    'offsetUnset' => function ($it) { unset($it[null]); return 'unset'; },
] as $name => $fn) {
    try { echo $name, ' => ', var_export($fn($it), true), "\n"; }
    catch (Throwable $e) { echo $name, ' => ', get_class($e), ': ', $e->getMessage(), "\n"; }
}
echo 'cache untouched => ', json_encode($it->getCache()), "\n";
echo 'int key => ', var_export(isset($it[0]), true), "\n";
foreach ([1.5, null] as $flags) {
    try { $o = new CachingIterator(new ArrayIterator([1]), $flags); echo 'flags ',
        var_export($flags, true), ' => ', $o->getFlags(), "\n"; }
    catch (Throwable $e) { echo 'flags ', var_export($flags, true), ' => ',
        get_class($e), ': ', $e->getMessage(), "\n"; }
}

/* RecursiveTreeIterator's own null door: php reads $cachingIteratorFlags as an
 * int, so a null becomes 0 and builds a wrapper that does not catch. */
try { $t = new RecursiveTreeIterator(new RecursiveArrayIterator([1]), 8, null, 1);
      $t->rewind();
      echo 'tree null flags => ', $t->getSubIterator()->getFlags(), "\n"; }
catch (Throwable $e) { echo 'tree null flags => ', get_class($e), ': ', $e->getMessage(), "\n"; }
?>
--EXPECT--
offsetGet => NULL
offsetExists => false
offsetSet => 'written'
append => 'appended'
offsetUnset => 'unset'
cache untouched => {"x":1}
int key => false
flags 1.5 => 1
flags NULL => 0
tree null flags => 65536
