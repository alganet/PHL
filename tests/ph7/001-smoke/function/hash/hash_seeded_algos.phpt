--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
hash() computes the seeded fast hashes (murmur3a/c/f, xxh32, xxh64) and reads $options['seed'] the way php does
--FILE--
<?php
foreach (['murmur3a','murmur3c','murmur3f','xxh32','xxh64'] as $a) {
    echo $a, " ''=", hash($a, ""), " abc=", hash($a, "abc"), "\n";
}
// A tail shorter than one block is a separate code path per algorithm, and
// 16/32 bytes is where the block loop starts answering instead of the seed.
foreach ([1, 3, 4, 15, 16, 17, 31, 32, 33] as $n) {
    echo "len$n xxh32=", hash('xxh32', str_repeat('z', $n)), " xxh64=", hash('xxh64', str_repeat('z', $n)), "\n";
}
// The seed is 32-bit for murmur3a/murmur3c/xxh32 and 64-bit for the other two,
// so 2^32+1 is the seed 1 for three of them and a different seed for two.
foreach (['murmur3a','murmur3c','murmur3f','xxh32','xxh64'] as $a) {
    $one = hash($a, "abc", false, ['seed' => 1]);
    $wide = hash($a, "abc", false, ['seed' => 4294967297]);
    echo $a, " seed1=", $one, " 32-bit-seed=", var_export($one === $wide, true), "\n";
}
// An unknown key is ignored by both engines (a non-int SEED is not: see the
// hash_seed_type twin pair in 002-integration).
$plain = hash('murmur3a', "abc");
echo "unknown key: ", var_export(hash('murmur3a', "abc", false, ['bogus' => 1]) === $plain, true), "\n";
echo "no options: ", var_export(hash('murmur3a', "abc", false, []) === $plain, true), "\n";
// A seed on an algorithm that has none changes nothing.
echo "md5 seeded: ", var_export(hash('md5', "abc", false, ['seed' => 42]) === hash('md5', "abc"), true), "\n";
// $options is declared array, and php refuses anything else.
try { hash('md5', "abc", false, "x"); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
// Not cryptographic either: a fast hash may not key a MAC.
try { hash_hmac('xxh64', "abc", "k"); } catch (ValueError $e) { echo $e->getMessage(), "\n"; }
echo "raw: ", strlen(hash('murmur3c', "abc", true)), " ", strlen(hash('xxh64', "abc", true)), "\n";
?>
--EXPECT--
murmur3a ''=00000000 abc=b3dd93fa
murmur3c ''=00000000000000000000000000000000 abc=75cdc6d1a2b006a5a2b006a5a2b006a5
murmur3f ''=00000000000000000000000000000000 abc=b4963f3f3fad78673ba2744126ca2d52
xxh32 ''=02cc5d05 abc=32d153ff
xxh64 ''=ef46db3751d8e999 abc=44bc2cf5ad770999
len1 xxh32=a73026ce xxh64=048a5a7677a8e488
len3 xxh32=96a23210 xxh64=6d85d478e2fa354b
len4 xxh32=e9be83c2 xxh64=045b93e210505f8c
len15 xxh32=7ebba956 xxh64=4857f9287fce717a
len16 xxh32=3e98833e xxh64=f1b185078caa9443
len17 xxh32=700f992b xxh64=fafce7d515795c84
len31 xxh32=cb437d48 xxh64=42021d0f39a63d19
len32 xxh32=10696730 xxh64=5f25dc5a433d2361
len33 xxh32=bd188335 xxh64=c5241253c64e0268
murmur3a seed1=aa75e9ff 32-bit-seed=true
murmur3c seed1=327c7bab21a3830021a3830021a38300 32-bit-seed=true
murmur3f seed1=9c88be4e9a8a61f0ca12c88bf31b256c 32-bit-seed=false
xxh32 seed1=aa3da8ff 32-bit-seed=true
xxh64 seed1=bea9ca8199328908 32-bit-seed=false
unknown key: true
no options: true
md5 seeded: true
hash(): Argument #4 ($options) must be of type array, string given
hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm
raw: 16 8
--CLEAN--
<?php
