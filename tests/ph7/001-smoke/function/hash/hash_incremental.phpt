--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
hash_init/hash_update/hash_final/hash_copy over a HashContext, including HMAC, the finalized-context refusal and what the object shows
--FILE--
<?php
// A digest may not depend on how the bytes were split.
$one = hash('sha256', "The quick brown fox jumps over the lazy dog");
$ctx = hash_init('sha256');
foreach (str_split("The quick brown fox jumps over the lazy dog", 7) as $chunk) {
    $r = hash_update($ctx, $chunk);
}
echo "update returns: ", var_export($r, true), "\n";
echo "split == whole: ", var_export(hash_final($ctx) === $one, true), "\n";
// Every algorithm, fed one byte at a time, agrees with the one-shot call.
$algos = ['md2','md4','md5','sha1','sha512/256','sha3-256','ripemd160','crc32b','adler32','joaat','murmur3f','xxh64'];
$data = "abcdefghijklmnopqrstuvwxyz0123456789!";
foreach ($algos as $a) {
    $c = hash_init($a);
    for ($i = 0; $i < strlen($data); $i++) { hash_update($c, $data[$i]); }
    echo $a, " ", var_export(hash_final($c) === hash($a, $data), true), "\n";
}
// An empty context is the empty string's digest.
echo "empty: ", var_export(hash_final(hash_init('md5')) === hash('md5', ""), true), "\n";
echo "raw: ", strlen(hash_final(hash_init('sha256'), true)), "\n";
// HASH_HMAC through the context is hash_hmac() by another route.
$c = hash_init('sha256', HASH_HMAC, 'the key');
hash_update($c, "msg");
echo "hmac: ", var_export(hash_final($c) === hash_hmac('sha256', "msg", 'the key'), true), "\n";
// A copy is a second context at the same point, not a second reference to one.
$c = hash_init('md5');
hash_update($c, "a");
$d = hash_copy($c);
hash_update($d, "b");
echo "copy: ", hash_final($c), " ", hash_final($d), "\n";
echo "copy-is-independent: ", var_export(hash_final(hash_copy(hash_init('md5'))) === hash('md5', ""), true), "\n";
// php frees the context in hash_final(), so everything after it is refused —
// and the refusal describes the ARGUMENT, so it is a TypeError.
$c = hash_init('md5');
hash_final($c);
foreach (['hash_update', 'hash_final', 'hash_copy'] as $fn) {
    try {
        $fn === 'hash_update' ? hash_update($c, "x") : $fn($c);
        echo $fn, " ACCEPTED\n";
    } catch (TypeError $e) {
        echo $e->getMessage(), "\n";
    }
}
// What is NOT a context at all is the declared type's refusal.
try { hash_update("ctx", "x"); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
// hash_init's own refusals, in php's order: the algorithm first, then whether
// it may be keyed, then the key itself.
foreach ([['nope', 0, ''], ['nope', HASH_HMAC, 'k'], ['crc32b', HASH_HMAC, 'k'],
          ['md5', HASH_HMAC, ''], ['md5', HASH_HMAC, null]] as [$a, $f, $k]) {
    try {
        hash_init($a, $f, (string)$k);
        echo "accepted\n";
    } catch (ValueError $e) {
        echo $e->getMessage(), "\n";
    }
}
// Only one bit of $flags is read, and a key without that bit is ignored.
echo "flag 99 is hmac: ", var_export(hash_final(hash_init('md5', 99, 'k')) === hash_hmac('md5', "", 'k'), true), "\n";
echo "key without flag: ", var_export(hash_final(hash_init('md5', 0, 'k')) === hash('md5', ""), true), "\n";
// The object itself: final, built only by hash_init(), and presenting one key
// to var_dump/print_r while the (array) cast and var_export show nothing.
$c = hash_init('md5');
echo get_class($c), " final=", var_export((new ReflectionClass('HashContext'))->isFinal(), true), "\n";
// (the object HANDLE is engine bookkeeping, not a php-visible answer)
ob_start(); var_dump($c); echo preg_replace('/#\d+/', '#N', ob_get_clean());
print_r($c);
var_dump((array)$c);
try { new HashContext(); } catch (Error $e) { echo $e->getMessage(), "\n"; }
// A clone is a copy of the state, exactly as hash_copy() is.
$c = hash_init('md5');
hash_update($c, "a");
$e = clone $c;
hash_update($e, "b");
echo "clone: ", hash_final($c), " ", hash_final($e), "\n";
?>
--EXPECT--
update returns: true
split == whole: true
md2 true
md4 true
md5 true
sha1 true
sha512/256 true
sha3-256 true
ripemd160 true
crc32b true
adler32 true
joaat true
murmur3f true
xxh64 true
empty: true
raw: 32
hmac: true
copy: 0cc175b9c0f1b6a831c399e269772661 187ef4436122d1cc2f40dc2b92f0eba0
copy-is-independent: true
hash_update(): Argument #1 ($context) must be a valid, non-finalized HashContext
hash_final(): Argument #1 ($context) must be a valid, non-finalized HashContext
hash_copy(): Argument #1 ($context) must be a valid, non-finalized HashContext
hash_update(): Argument #1 ($context) must be of type HashContext, string given
hash_init(): Argument #1 ($algo) must be a valid hashing algorithm
hash_init(): Argument #1 ($algo) must be a valid hashing algorithm
hash_init(): Argument #1 ($algo) must be a cryptographic hashing algorithm if HMAC is requested
hash_init(): Argument #3 ($key) must not be empty when HMAC is requested
hash_init(): Argument #3 ($key) must not be empty when HMAC is requested
flag 99 is hmac: true
key without flag: true
HashContext final=true
object(HashContext)#N (1) {
  ["algo"]=>
  string(3) "md5"
}
HashContext Object
(
    [algo] => md5
)
array(0) {
}
Call to private HashContext::__construct() from global scope
clone: 0cc175b9c0f1b6a831c399e269772661 187ef4436122d1cc2f40dc2b92f0eba0
--CLEAN--
<?php
