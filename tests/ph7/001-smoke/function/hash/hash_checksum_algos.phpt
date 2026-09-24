--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
hash() computes the checksum family (crc32/crc32b/crc32c/adler32/fnv/joaat), which is exactly the set hash_hmac() refuses
--FILE--
<?php
// The check-value every CRC specification is published with, plus the empty
// string (whose answer is the initial register, not zero, for three of these).
foreach (['crc32','crc32b','crc32c','adler32','fnv132','fnv1a32','fnv164','fnv1a64','joaat'] as $a) {
    echo $a, " 123456789=", hash($a, "123456789"), " ''=", hash($a, ""), " a=", hash($a, "a"), "\n";
}
// crc32() the function and 'crc32b' the algorithm are the same number.
echo "crc32b==crc32(): ", var_export(sprintf('%08x', crc32("The quick brown fox")) === hash('crc32b', "The quick brown fox"), true), "\n";
// A digest is a digest however the bytes arrive: 4/8 raw bytes, hex otherwise.
echo "raw lengths: ", strlen(hash('crc32b', "x", true)), " ", strlen(hash('fnv164', "x", true)), "\n";
// Not cryptographic: php refuses to key any of them, and keeps them out of the
// MAC list while advertising them in the general one.
$algos = hash_algos();
$macs = hash_hmac_algos();
foreach (['crc32','crc32b','crc32c','adler32','fnv132','fnv1a32','fnv164','fnv1a64','joaat'] as $a) {
    $listed = in_array($a, $algos, true) && !in_array($a, $macs, true);
    try {
        hash_hmac($a, "data", "key");
        echo $a, " KEYED\n";
    } catch (ValueError $e) {
        echo $a, ($listed ? " listed-not-mac" : " LISTING-WRONG"), ": ", $e->getMessage(), "\n";
    }
}
foreach (['md5','sha256','sha512'] as $a) {
    echo $a, " mac-able: ", var_export(in_array($a, $macs, true), true), "\n";
}
?>
--EXPECT--
crc32 123456789=181989fc ''=00000000 a=6b9b9319
crc32b 123456789=cbf43926 ''=00000000 a=e8b7be43
crc32c 123456789=e3069283 ''=00000000 a=c1d04330
adler32 123456789=091e01de ''=00000001 a=00620062
fnv132 123456789=24148816 ''=811c9dc5 a=050c5d7e
fnv1a32 123456789=bb86b11c ''=811c9dc5 a=e40c292c
fnv164 123456789=a72ffc362bf916d6 ''=cbf29ce484222325 a=af63bd4c8601b7be
fnv1a64 123456789=06d5573923c6cdfc ''=cbf29ce484222325 a=af63dc4c8601ec8c
joaat 123456789=c66b58c5 ''=00000000 a=ca2e9442
crc32b==crc32(): true
raw lengths: 4 8
crc32 listed-not-mac: hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm
crc32b listed-not-mac: hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm
crc32c listed-not-mac: hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm
adler32 listed-not-mac: hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm
fnv132 listed-not-mac: hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm
fnv1a32 listed-not-mac: hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm
fnv164 listed-not-mac: hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm
fnv1a64 listed-not-mac: hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm
joaat listed-not-mac: hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm
md5 mac-able: true
sha256 mac-able: true
sha512 mac-able: true
--CLEAN--
<?php
