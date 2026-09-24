--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
hash_pbkdf2() and hash_hkdf() derive keys, with the published RFC vectors and php's own length rules
--FILE--
<?php
// RFC 6070's PBKDF2-HMAC-SHA1 vectors.
echo hash_pbkdf2('sha1', 'password', 'salt', 1, 40), "\n";
echo hash_pbkdf2('sha1', 'password', 'salt', 2, 40), "\n";
echo hash_pbkdf2('sha1', 'password', 'salt', 4096, 40), "\n";
// RFC 5869's HKDF-SHA256 test case 1.
echo bin2hex(hash_hkdf('sha256', hex2bin('0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b'), 42,
    hex2bin('f0f1f2f3f4f5f6f7f8f9'), hex2bin('000102030405060708090a0b0c'))), "\n";
// $length counts what the OUTPUT is made of: bytes when raw, hex characters
// otherwise — so an odd length cuts a byte in half and keeps the high nibble.
foreach ([0, 1, 7, 32, 33, 64, 65] as $len) {
    echo "len$len hex=", strlen(hash_pbkdf2('sha256', 'p', 's', 2, $len)),
         " raw=", strlen(hash_pbkdf2('sha256', 'p', 's', 2, $len, true)), "\n";
}
echo "odd is the high nibble: ",
    var_export(hash_pbkdf2('sha256', 'p', 's', 2, 7) === substr(hash_pbkdf2('sha256', 'p', 's', 2, 8), 0, 7), true), "\n";
echo "zero is the digest: ",
    var_export(strlen(hash_pbkdf2('sha512', 'p', 's', 2)) === 128, true), "\n";
// More than one block is the counter's job, and it is four big-endian bytes.
echo "blocks: ", var_export(substr(hash_pbkdf2('md5', 'p', 's', 3, 96), 0, 32)
    === hash_pbkdf2('md5', 'p', 's', 3, 32), true), "\n";
// hash_hkdf always answers RAW bytes, and its ceiling is 255 blocks because
// its counter is ONE byte.
echo "hkdf default: ", strlen(hash_hkdf('sha256', 'key')), "\n";
echo "hkdf max: ", strlen(hash_hkdf('sha256', 'key', 255 * 32)), "\n";
echo "hkdf salt matters: ", var_export(hash_hkdf('sha256', 'key', 32, 'info', 'salt')
    !== hash_hkdf('sha256', 'key', 32, 'info'), true), "\n";
echo "hkdf info matters: ", var_export(hash_hkdf('sha256', 'key', 32, 'info')
    !== hash_hkdf('sha256', 'key', 32), true), "\n";
// Both are keyed, so both refuse an algorithm that is not cryptographic.
foreach ([fn() => hash_pbkdf2('crc32b', 'p', 's', 1),
          fn() => hash_pbkdf2('nope', 'p', 's', 1),
          fn() => hash_pbkdf2('md5', 'p', 's', 0),
          fn() => hash_pbkdf2('md5', 'p', 's', -1),
          fn() => hash_pbkdf2('md5', 'p', 's', 1, -1),
          fn() => hash_hkdf('crc32b', 'k'),
          fn() => hash_hkdf('nope', 'k'),
          fn() => hash_hkdf('md5', ''),
          fn() => hash_hkdf('md5', 'k', -1),
          fn() => hash_hkdf('md5', 'k', 255 * 16 + 1),
          fn() => hash_hkdf('sha256', 'k', 255 * 32 + 1)] as $case) {
    try { $case(); echo "accepted\n"; } catch (ValueError $e) { echo $e->getMessage(), "\n"; }
}
?>
--EXPECT--
0c60c80f961f0e71f3a9b524af6012062fe037a6
ea6c014dc72d6f8ccd1ed92ace1d41f0d8de8957
4b007901b765489abead49d926f721d065a429c1
8a343ebf7af154aef74eb9befa06127aefc81ce04df181d10ceca10853eda9ec0255f649fa8f7f6e9e66
len0 hex=64 raw=32
len1 hex=1 raw=1
len7 hex=7 raw=7
len32 hex=32 raw=32
len33 hex=33 raw=33
len64 hex=64 raw=64
len65 hex=65 raw=65
odd is the high nibble: true
zero is the digest: true
blocks: true
hkdf default: 32
hkdf max: 8160
hkdf salt matters: true
hkdf info matters: true
hash_pbkdf2(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm
hash_pbkdf2(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm
hash_pbkdf2(): Argument #4 ($iterations) must be greater than 0
hash_pbkdf2(): Argument #4 ($iterations) must be greater than 0
hash_pbkdf2(): Argument #5 ($length) must be greater than or equal to 0
hash_hkdf(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm
hash_hkdf(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm
hash_hkdf(): Argument #2 ($key) must not be empty
hash_hkdf(): Argument #3 ($length) must be greater than or equal to 0
hash_hkdf(): Argument #3 ($length) must be less than or equal to 4080
hash_hkdf(): Argument #3 ($length) must be less than or equal to 8160
--CLEAN--
<?php
