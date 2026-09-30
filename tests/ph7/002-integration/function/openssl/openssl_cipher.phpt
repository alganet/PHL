--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/openssl: what a cipher answers, and every sentence it refuses with
--FILE--
<?php
/* ext/openssl's symmetric half: what a cipher answers, and what it says when
 * it refuses. Nothing here prints a name out of the LIBRARY's algorithm tables
 * or a version string -- those move with the OpenSSL this build linked (3.0 on
 * one box, 3.6 on another) and are asserted by shape below rather than by
 * value. The ciphertexts are AES's own and are the same everywhere. */
set_error_handler(static function ($n, $m) { echo 'W: ', $m, "\n"; return true; });
$key = str_repeat('k', 32);
$iv  = str_repeat('i', 16);

echo "-- the two length questions\n";
foreach (['aes-256-cbc', 'aes-128-ecb', 'aes-256-gcm', 'chacha20-poly1305', 'nosuchcipher'] as $c) {
    printf("%-18s iv=%s key=%s\n", $c,
        var_export(@openssl_cipher_iv_length($c), true),
        var_export(@openssl_cipher_key_length($c), true));
}
try { openssl_cipher_iv_length(''); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { openssl_cipher_key_length(''); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

echo "-- a round trip, base64 and raw\n";
$b64 = openssl_encrypt('hello world', 'aes-256-cbc', $key, 0, $iv);
$raw = openssl_encrypt('hello world', 'aes-256-cbc', $key, OPENSSL_RAW_DATA, $iv);
var_dump($b64, bin2hex($raw), base64_decode($b64) === $raw);
var_dump(openssl_decrypt($b64, 'aes-256-cbc', $key, 0, $iv));
var_dump(openssl_decrypt($raw, 'aes-256-cbc', $key, OPENSSL_RAW_DATA, $iv));

echo "-- the IV screen: three sentences, none of them an error\n";
var_dump(openssl_encrypt('hello', 'aes-256-cbc', $key, 0, ''));
var_dump(openssl_encrypt('hello', 'aes-256-cbc', $key, 0, 'ab'));
var_dump(openssl_encrypt('hello', 'aes-256-cbc', $key, 0, str_repeat('i', 24)));
echo "   and a cipher that wants none still reports the one it was handed\n";
var_dump(openssl_encrypt('hello', 'aes-256-ecb', $key, 0, 'ab'));
echo "   the empty-IV sentence is ENCRYPT-only\n";
var_dump(bin2hex((string) openssl_decrypt($raw, 'aes-256-cbc', $key, OPENSSL_RAW_DATA, '')));
var_dump(bin2hex((string) openssl_decrypt($raw, 'aes-256-cbc', $key, OPENSSL_RAW_DATA, str_repeat('i', 24))));

echo "-- the key screen\n";
var_dump(openssl_encrypt('hello', 'aes-256-cbc', 'k', OPENSSL_RAW_DATA, $iv) ===
         openssl_encrypt('hello', 'aes-256-cbc', 'k' . str_repeat("\0", 31), OPENSSL_RAW_DATA, $iv));
var_dump(openssl_encrypt('hello world', 'aes-256-cbc', $key . 'excess', OPENSSL_RAW_DATA, $iv) === $raw);
var_dump(openssl_encrypt('hello', 'aes-256-cbc', 'k', OPENSSL_DONT_ZERO_PAD_KEY, $iv));
echo "   a long passphrase leaves the refusal it ignored in the error ring\n";
var_dump(is_string(openssl_error_string()) || openssl_error_string() === false);
while (openssl_error_string() !== false) { /* drain whatever the refusals above left */ }
openssl_encrypt('hello', 'aes-256-cbc', $key . 'excess', OPENSSL_RAW_DATA, $iv);
var_dump(is_string(openssl_error_string()));

echo "-- padding\n";
var_dump(bin2hex((string) openssl_encrypt(str_repeat('a', 16), 'aes-256-cbc', $key,
    OPENSSL_RAW_DATA | OPENSSL_ZERO_PADDING, $iv)));
var_dump(openssl_encrypt('hello', 'aes-256-cbc', $key, OPENSSL_RAW_DATA | OPENSSL_ZERO_PADDING, $iv));
var_dump(bin2hex((string) openssl_decrypt($raw, 'aes-256-cbc', $key,
    OPENSSL_RAW_DATA | OPENSSL_ZERO_PADDING, $iv)));

echo "-- AEAD: the tag is an out-parameter\n";
$tag = null;
$gcm = openssl_encrypt('hello', 'aes-256-gcm', $key, OPENSSL_RAW_DATA, substr($iv, 0, 12), $tag, 'aad', 16);
var_dump(bin2hex($gcm), bin2hex($tag));
var_dump(openssl_decrypt($gcm, 'aes-256-gcm', $key, OPENSSL_RAW_DATA, substr($iv, 0, 12), $tag, 'aad'));
echo "   a wrong aad, a wrong tag and no tag at all are all a silent false\n";
var_dump(openssl_decrypt($gcm, 'aes-256-gcm', $key, OPENSSL_RAW_DATA, substr($iv, 0, 12), $tag, 'other'));
var_dump(openssl_decrypt($gcm, 'aes-256-gcm', $key, OPENSSL_RAW_DATA, substr($iv, 0, 12), str_repeat("\0", 16), 'aad'));
var_dump(openssl_decrypt($gcm, 'aes-256-gcm', $key, OPENSSL_RAW_DATA, substr($iv, 0, 12), null, 'aad'));
echo "   a 17-byte tag is the one the cipher refuses\n";
var_dump(openssl_decrypt($gcm, 'aes-256-gcm', $key, OPENSSL_RAW_DATA, substr($iv, 0, 12), str_repeat('a', 17), 'aad'));
echo "   an empty nonce is the IV-length refusal, not the empty-IV warning\n";
$t2 = null;
var_dump(openssl_encrypt('hello', 'aes-256-gcm', $key, OPENSSL_RAW_DATA, '', $t2));
echo "   a tag length outside 4..16 fails the retrieval\n";
$t3 = null;
var_dump(openssl_encrypt('hello', 'aes-256-gcm', $key, OPENSSL_RAW_DATA, substr($iv, 0, 12), $t3, '', 0));
$t4 = null;
var_dump(bin2hex((string) openssl_encrypt('hello', 'aes-256-gcm', $key, OPENSSL_RAW_DATA,
    substr($iv, 0, 12), $t4, '', 4)), strlen((string) $t4));
echo "   chacha20-poly1305 is AEAD without being one of the three MODES\n";
$t5 = null;
$cc = openssl_encrypt('hello', 'chacha20-poly1305', $key, OPENSSL_RAW_DATA, substr($iv, 0, 12), $t5, 'aad');
var_dump(bin2hex((string) $cc), strlen((string) $t5));
var_dump(openssl_decrypt($cc, 'chacha20-poly1305', $key, OPENSSL_RAW_DATA, substr($iv, 0, 12), $t5, 'aad'));
echo "   CCM verifies inside its Update: no Final, and the tag precedes the key\n";
$t6 = null;
$ccm = openssl_encrypt('hello', 'aes-256-ccm', $key, OPENSSL_RAW_DATA, substr($iv, 0, 12), $t6, 'aad', 16);
var_dump(bin2hex((string) $ccm), strlen((string) $t6));
var_dump(openssl_decrypt($ccm, 'aes-256-ccm', $key, OPENSSL_RAW_DATA, substr($iv, 0, 12), $t6, 'aad'));
echo "   an empty plaintext round-trips through every one of them\n";
foreach (['aes-256-gcm', 'chacha20-poly1305', 'aes-256-ccm'] as $c) {
    $t = null;
    $e = openssl_encrypt('', $c, $key, OPENSSL_RAW_DATA, substr($iv, 0, 12), $t, '', 16);
    printf("%-18s %s %s\n", $c, var_export($e, true),
        var_export(openssl_decrypt($e, $c, $key, OPENSSL_RAW_DATA, substr($iv, 0, 12), $t, ''), true));
}

echo "-- a tag on a cipher that has none is a WARNING and not a refusal\n";
var_dump(openssl_decrypt($raw, 'aes-256-cbc', $key, OPENSSL_RAW_DATA, $iv, 'tag'));
$t7 = 'kept';
var_dump(openssl_encrypt('hello', 'aes-256-cbc', $key, OPENSSL_RAW_DATA, $iv, $t7) === $raw, $t7);

echo "-- an unknown cipher, and an empty name\n";
var_dump(openssl_encrypt('x', 'nosuchcipher', $key, 0, $iv));
var_dump(openssl_encrypt('x', '', $key, 0, $iv));
var_dump(openssl_decrypt('x', 'nosuchcipher', $key, 0, $iv));

echo "-- a base64 body that is not base64 decodes to nothing\n";
var_dump(openssl_decrypt('!!!', 'aes-256-cbc', $key, 0, $iv));
var_dump(openssl_decrypt('abc', 'aes-256-cbc', $key, OPENSSL_RAW_DATA, $iv));

restore_error_handler();
--EXPECT--
-- the two length questions
aes-256-cbc        iv=16 key=32
aes-128-ecb        iv=0 key=16
aes-256-gcm        iv=12 key=32
chacha20-poly1305  iv=12 key=32
W: openssl_cipher_iv_length(): Unknown cipher algorithm
W: openssl_cipher_key_length(): Unknown cipher algorithm
nosuchcipher       iv=false key=false
ValueError: openssl_cipher_iv_length(): Argument #1 ($cipher_algo) must not be empty
ValueError: openssl_cipher_key_length(): Argument #1 ($cipher_algo) must not be empty
-- a round trip, base64 and raw
string(24) "ioKNFMhJ1U5QPG7Ri3bZnA=="
string(32) "8a828d14c849d54e503c6ed18b76d99c"
bool(true)
string(11) "hello world"
string(11) "hello world"
-- the IV screen: three sentences, none of them an error
W: openssl_encrypt(): Using an empty Initialization Vector (iv) is potentially insecure and not recommended
string(24) "9AVgiDpwg33v/jPjj++ZYA=="
W: openssl_encrypt(): IV passed is only 2 bytes long, cipher expects an IV of precisely 16 bytes, padding with \0
string(24) "kbM7TzNORolusXr0rIn/LQ=="
W: openssl_encrypt(): IV passed is 24 bytes long which is longer than the 16 expected by selected cipher, truncating
string(24) "aRyXOIIhJzBhh+BthfFstw=="
   and a cipher that wants none still reports the one it was handed
W: openssl_encrypt(): IV passed is 2 bytes long which is longer than the 0 expected by selected cipher, truncating
string(24) "9AVgiDpwg33v/jPjj++ZYA=="
   the empty-IV sentence is ENCRYPT-only
string(0) ""
W: openssl_decrypt(): IV passed is 24 bytes long which is longer than the 16 expected by selected cipher, truncating
string(22) "68656c6c6f20776f726c64"
-- the key screen
bool(true)
bool(true)
W: openssl_encrypt(): Key length cannot be set for the cipher algorithm
bool(false)
   a long passphrase leaves the refusal it ignored in the error ring
bool(true)
bool(true)
-- padding
string(32) "6050357baa596ee77ea5e81751dc2ce3"
bool(false)
string(32) "68656c6c6f20776f726c640505050505"
-- AEAD: the tag is an out-parameter
string(10) "b2e194185d"
string(32) "230b7e4f31933ad3a4b34184de170983"
string(5) "hello"
   a wrong aad, a wrong tag and no tag at all are all a silent false
bool(false)
bool(false)
bool(false)
   a 17-byte tag is the one the cipher refuses
W: openssl_decrypt(): Setting tag for AEAD cipher decryption failed
bool(false)
   an empty nonce is the IV-length refusal, not the empty-IV warning
W: openssl_encrypt(): Setting of IV length for AEAD mode failed
bool(false)
   a tag length outside 4..16 fails the retrieval
W: openssl_encrypt(): Retrieving verification tag failed
bool(false)
string(10) "b2e194185d"
int(4)
   chacha20-poly1305 is AEAD without being one of the three MODES
string(10) "9be2657e40"
int(16)
string(5) "hello"
   CCM verifies inside its Update: no Final, and the tag precedes the key
string(10) "9a75402a27"
int(16)
string(5) "hello"
   an empty plaintext round-trips through every one of them
aes-256-gcm        '' ''
chacha20-poly1305  '' ''
aes-256-ccm        '' ''
-- a tag on a cipher that has none is a WARNING and not a refusal
W: openssl_decrypt(): The tag cannot be used because the cipher algorithm does not support AEAD
string(11) "hello world"
bool(false)
NULL
-- an unknown cipher, and an empty name
W: openssl_encrypt(): Unknown cipher algorithm
bool(false)
W: openssl_encrypt(): Unknown cipher algorithm
bool(false)
W: openssl_decrypt(): Unknown cipher algorithm
bool(false)
-- a base64 body that is not base64 decodes to nothing
bool(false)
bool(false)
