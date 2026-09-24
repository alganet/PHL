--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: a HashContext round-trips through serialize(), in PHL's own payload layout
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* Both engines serialize a HashContext and both round-trip it; what neither
 * can do is read the OTHER's payload, because each writes its own internal
 * context. php's is five entries of engine state, PHL's is the algorithm's
 * name and the running state's bytes. See the zend half. */
$c = hash_init('sha256');
hash_update($c, "The quick brown ");
$parked = serialize($c);
$resumed = unserialize($parked);
hash_update($resumed, "fox");
var_dump(hash_final($resumed) === hash('sha256', "The quick brown fox"));
/* the parked context is untouched by the resumed one */
hash_update($c, "cat");
var_dump(hash_final($c) === hash('sha256', "The quick brown cat"));
/* an HMAC context is refused outright by BOTH engines, and the reason is the
 * point: its state carries the KEY, so a payload would carry it in clear */
try { serialize(hash_init('sha256', HASH_HMAC, 'the key')); }
catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
/* and so is a context whose digest has already been taken */
$done = hash_init('md5'); hash_final($done);
try { serialize($done); } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
/* the payload is UNTRUSTED: a state whose bytes were tampered with is refused
 * before anything can be driven from it */
$parts = hash_init('sha256')->__serialize();
echo count($parts), " ", $parts[0], " ", strlen($parts[1]) > 0 ? "state" : "empty", "\n";
/* __unserialize exists for the object the UNSERIALIZER builds, so a context
 * that already has state refuses before it looks at the payload — which is
 * php's guard too, and what makes the tampering below unreachable from a
 * script that did not go through unserialize() */
try { hash_init('md5')->__unserialize($parts); }
catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
/* the payload IS untrusted where the unserializer hands it over, so a state
 * whose bytes were tampered with is refused before anything is driven from it:
 * every context here carries a cursor into a fixed buffer */
$tampered = $parts[1];
$tampered[0] = "\xff";
foreach ([[], ['sha256'], ['sha256', 'short'], ['nosuch', $parts[1]], [1, 2],
          ['sha256', $tampered], ['md5', $parts[1]]] as $bad) {
    try {
        unserialize('O:11:"HashContext":2:{i:0;' . serialize($bad[0] ?? null)
            . 'i:1;' . serialize($bad[1] ?? null) . '}');
        echo "accepted\n";
    } catch (Throwable $e) {
        echo get_class($e), ": ", $e->getMessage(), "\n";
    }
}
?>
--EXPECT--
bool(true)
bool(true)
Exception: HashContext with HASH_HMAC option cannot be serialized
Exception: HashContext for algorithm "md5" cannot be serialized
2 sha256 state
Exception: HashContext::__unserialize called on initialized object
Error: Invalid serialization data for HashContext object
Error: Invalid serialization data for HashContext object
Error: Invalid serialization data for HashContext object
Exception: Unknown hash algorithm
Error: Invalid serialization data for HashContext object
Exception: Unknown hash algorithm
Exception: Unknown hash algorithm
--CLEAN--
<?php
