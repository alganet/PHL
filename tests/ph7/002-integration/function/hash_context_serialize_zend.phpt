--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php: a HashContext round-trips through serialize(), in php's own five-entry payload layout (zend half of the twin pair)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* Both engines round-trip their own payload and refuse the same two contexts;
 * what neither can do is read the OTHER's, because each writes its own
 * internal state. php's __serialize is FIVE entries (the algorithm, its
 * options, the context as an array of machine words, a magic number and the
 * options array); PHL's is two — the algorithm's name and the state's bytes.
 * See the PHL half of the pair. */
$c = hash_init('sha256');
hash_update($c, "The quick brown ");
$resumed = unserialize(serialize($c));
hash_update($resumed, "fox");
var_dump(hash_final($resumed) === hash('sha256', "The quick brown fox"));
hash_update($c, "cat");
var_dump(hash_final($c) === hash('sha256', "The quick brown cat"));
/* an HMAC context is refused outright by BOTH engines, and the reason is the
 * point: its state carries the KEY, so a payload would carry it in clear */
try { serialize(hash_init('sha256', HASH_HMAC, 'the key')); }
catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
/* and so is a context whose digest has already been taken */
$done = hash_init('md5'); hash_final($done);
try { serialize($done); } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
/* the payload SHAPE is what the two engines disagree about */
$parts = hash_init('sha256')->__serialize();
echo count($parts), " ", $parts[0], "\n";
/* __unserialize exists for the object the UNSERIALIZER builds, so a context
 * that already has state refuses before it looks at the payload */
try { hash_init('md5')->__unserialize($parts); }
catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
?>
--EXPECT--
bool(true)
bool(true)
Exception: HashContext with HASH_HMAC option cannot be serialized
Exception: HashContext for algorithm "md5" cannot be serialized
5 sha256
Exception: HashContext::__unserialize called on initialized object
--CLEAN--
<?php
