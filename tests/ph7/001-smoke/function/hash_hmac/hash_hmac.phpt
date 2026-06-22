--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
hash_hmac() RFC 2104 keyed digests, incl. key longer than the block and empty key
--FILE--
<?php
echo hash_hmac("md5",  "data", "key"), "\n";
echo hash_hmac("sha1", "data", "key"), "\n";
echo hash_hmac("sha256", "The quick brown fox jumps over the lazy dog", "key"), "\n";
echo hash_hmac("sha512", "data", "key"), "\n";
// key longer than the 64-byte block is hashed down first
echo hash_hmac("sha256", "data", str_repeat("k", 200)), "\n";
// empty key
echo hash_hmac("sha256", "data", ""), "\n";
// raw output length
echo strlen(hash_hmac("sha256", "d", "k", true)), "\n";
?>
--EXPECT--
9d5c73ef85594d34ec4438b7c97e51d8
104152c5bfdca07bc633eebd46199f0255c9f49d
f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8
3c5953a18f7303ec653ba170ae334fafa08e3846f2efe317b87efce82376253cb52a8c31ddcde5a3a2eee183c2b34cb91f85e64ddbc325f7692b199473579c58
0163658d479ec73f2b801df07141db1426aaae2c6b5e8e6d9e6789f92f5e2b4f
e528c4d99e6177f5841f712a143b90843299a4aa181a06501422d9ca862bd2a5
32
--CLEAN--
<?php
