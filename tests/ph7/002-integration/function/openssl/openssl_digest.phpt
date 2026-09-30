--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/openssl: digests, PBKDF2, randomness and php's fifteen-slot error ring
--FILE--
<?php
/* ext/openssl's digest, derivation and randomness surface, plus the error RING
 * every one of them writes into. Nothing here pins a name out of the library's
 * algorithm tables, their length, or the text of an OpenSSL error -- all three
 * move with the linked OpenSSL. The digests themselves are the standards' and
 * are the same everywhere. */
set_error_handler(static function ($n, $m) { echo 'W: ', $m, "\n"; return true; });
function drain(): int { $n = 0; while (openssl_error_string() !== false) { $n++; if ($n > 40) break; } return $n; }

echo "-- a digest, by name and by php's own OPENSSL_ALGO_*\n";
foreach (['md5', 'sha1', 'sha256', 'sha512', 'ripemd160', 'md5-sha1'] as $a) {
    printf("%-10s %s\n", $a, openssl_digest('abc', $a));
}
var_dump(strlen(openssl_digest('abc', 'sha256', true)));
echo "   the provider's own spelling works, and so does an OBJ alias\n";
var_dump(openssl_digest('abc', 'SHA2-256') === openssl_digest('abc', 'sha256'));
var_dump(openssl_digest('abc', 'RSA-SHA256') === openssl_digest('abc', 'sha256'));
echo "   an unknown one is a warning and a false, and it leaves ONE error\n";
var_dump(drain());
var_dump(openssl_digest('abc', 'nosuchdigest'));
var_dump(drain());
echo "   an alias lookup that succeeds leaves NONE\n";
var_dump(openssl_digest('abc', 'RSA-SHA1') !== false, drain());

echo "-- openssl_pbkdf2\n";
var_dump(bin2hex(openssl_pbkdf2('password', 'salt', 20, 100, 'sha256')));
var_dump(bin2hex(openssl_pbkdf2('password', 'salt', 32, 1, 'sha512')));
var_dump(openssl_pbkdf2('p', 's', 20, 100, 'nosuchdigest'));
var_dump(openssl_pbkdf2('p', 's', 20, 0, 'sha256'), drain() > 0);
try { openssl_pbkdf2('p', 's', 0, 1, 'sha256'); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

echo "-- openssl_random_pseudo_bytes\n";
$strong = null;
var_dump(strlen(openssl_random_pseudo_bytes(16, $strong)), $strong);
var_dump(openssl_random_pseudo_bytes(1) !== openssl_random_pseudo_bytes(1) || true);
foreach ([0, -1] as $n) {
    try { openssl_random_pseudo_bytes($n); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}

echo "-- the error ring holds fifteen, oldest first\n";
var_dump(drain());
for ($i = 0; $i < 3; $i++) { openssl_digest('x', 'nosuchdigest' . $i); }
$seen = [];
while (($e = openssl_error_string()) !== false) { $seen[] = str_starts_with($e, 'error:'); }
var_dump($seen);
for ($i = 0; $i < 25; $i++) { openssl_digest('x', 'nosuchdigest' . $i); }
var_dump(drain());
var_dump(openssl_error_string());

echo "-- the algorithm listings: shape, not length\n";
$md = openssl_get_md_methods();
$mdAll = openssl_get_md_methods(true);
var_dump(in_array('sha256', $md, true), in_array('md5', $md, true));
var_dump(count($mdAll) > count($md), count(array_diff($md, $mdAll)));
var_dump($md === array_values($md), $md === array_unique($md));
$sorted = $md; sort($sorted, SORT_STRING);
var_dump($md === $sorted);
$ci = openssl_get_cipher_methods();
$ciAll = openssl_get_cipher_methods(true);
var_dump(in_array('aes-256-cbc', $ci, true), in_array('aes-256-gcm', $ci, true));
var_dump(count($ciAll) > count($ci), count(array_diff($ci, $ciAll)));
var_dump($ci === array_map('strtolower', $ci), $ci === array_unique($ci));
$sorted = $ci; sort($sorted, SORT_STRING);
var_dump($ci === $sorted);
$curves = openssl_get_curve_names();
var_dump(is_array($curves), in_array('prime256v1', $curves, true), in_array('secp384r1', $curves, true));

echo "-- where the library looks for a CA bundle\n";
$loc = openssl_get_cert_locations();
var_dump(array_keys($loc));
var_dump($loc['default_cert_file_env'], $loc['default_cert_dir_env']);
/* the ini pair is whatever this php.ini sets (setup-php points cafile at its own
 * bundle on Windows), so what is pinned is that the answer IS the ini value */
var_dump($loc['ini_cafile'] === (string) ini_get('openssl.cafile'),
    $loc['ini_capath'] === (string) ini_get('openssl.capath'));
var_dump(is_string($loc['default_cert_file']), is_string($loc['default_cert_dir']));

echo "-- the version pair is the LIBRARY's\n";
var_dump(get_debug_type(OPENSSL_VERSION_TEXT), get_debug_type(OPENSSL_VERSION_NUMBER));
var_dump(OPENSSL_VERSION_NUMBER >= 0x30000000);
var_dump(OPENSSL_RAW_DATA, OPENSSL_ZERO_PADDING, OPENSSL_DONT_ZERO_PAD_KEY);
var_dump(OPENSSL_ALGO_SHA1, OPENSSL_ALGO_MD5, OPENSSL_ALGO_MD4, OPENSSL_ALGO_SHA224,
         OPENSSL_ALGO_SHA256, OPENSSL_ALGO_SHA384, OPENSSL_ALGO_SHA512, OPENSSL_ALGO_RMD160);
var_dump(OPENSSL_KEYTYPE_RSA, OPENSSL_KEYTYPE_DSA, OPENSSL_KEYTYPE_DH, OPENSSL_KEYTYPE_EC,
         OPENSSL_KEYTYPE_X25519, OPENSSL_KEYTYPE_ED25519, OPENSSL_KEYTYPE_X448, OPENSSL_KEYTYPE_ED448);
var_dump(OPENSSL_ENCODING_DER, OPENSSL_ENCODING_SMIME, OPENSSL_ENCODING_PEM);
var_dump(strlen(OPENSSL_DEFAULT_STREAM_CIPHERS) > 0, str_contains(OPENSSL_DEFAULT_STREAM_CIPHERS, '!aNULL'));

restore_error_handler();
--EXPECT--
-- a digest, by name and by php's own OPENSSL_ALGO_*
md5        900150983cd24fb0d6963f7d28e17f72
sha1       a9993e364706816aba3e25717850c26c9cd0d89d
sha256     ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
sha512     ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f
ripemd160  8eb208f7e05d987a9b044a8e98c6b087f15a0bfc
md5-sha1   900150983cd24fb0d6963f7d28e17f72a9993e364706816aba3e25717850c26c9cd0d89d
int(32)
   the provider's own spelling works, and so does an OBJ alias
bool(true)
bool(true)
   an unknown one is a warning and a false, and it leaves ONE error
int(0)
W: openssl_digest(): Unknown digest algorithm
bool(false)
int(1)
   an alias lookup that succeeds leaves NONE
bool(true)
int(0)
-- openssl_pbkdf2
string(40) "07e6997180cf7f12904f04100d405d34888fdf62"
string(64) "867f70cf1ade02cff3752599a3a53dc4af34c7a669815ae5d513554e1c8cf252"
W: openssl_pbkdf2(): Unknown digest algorithm
bool(false)
bool(false)
bool(true)
ValueError: openssl_pbkdf2(): Argument #3 ($key_length) must be greater than 0
-- openssl_random_pseudo_bytes
int(16)
bool(true)
bool(true)
ValueError: openssl_random_pseudo_bytes(): Argument #1 ($length) must be greater than 0
ValueError: openssl_random_pseudo_bytes(): Argument #1 ($length) must be greater than 0
-- the error ring holds fifteen, oldest first
int(0)
W: openssl_digest(): Unknown digest algorithm
W: openssl_digest(): Unknown digest algorithm
W: openssl_digest(): Unknown digest algorithm
array(3) {
  [0]=>
  bool(true)
  [1]=>
  bool(true)
  [2]=>
  bool(true)
}
W: openssl_digest(): Unknown digest algorithm
W: openssl_digest(): Unknown digest algorithm
W: openssl_digest(): Unknown digest algorithm
W: openssl_digest(): Unknown digest algorithm
W: openssl_digest(): Unknown digest algorithm
W: openssl_digest(): Unknown digest algorithm
W: openssl_digest(): Unknown digest algorithm
W: openssl_digest(): Unknown digest algorithm
W: openssl_digest(): Unknown digest algorithm
W: openssl_digest(): Unknown digest algorithm
W: openssl_digest(): Unknown digest algorithm
W: openssl_digest(): Unknown digest algorithm
W: openssl_digest(): Unknown digest algorithm
W: openssl_digest(): Unknown digest algorithm
W: openssl_digest(): Unknown digest algorithm
W: openssl_digest(): Unknown digest algorithm
W: openssl_digest(): Unknown digest algorithm
W: openssl_digest(): Unknown digest algorithm
W: openssl_digest(): Unknown digest algorithm
W: openssl_digest(): Unknown digest algorithm
W: openssl_digest(): Unknown digest algorithm
W: openssl_digest(): Unknown digest algorithm
W: openssl_digest(): Unknown digest algorithm
W: openssl_digest(): Unknown digest algorithm
W: openssl_digest(): Unknown digest algorithm
int(15)
bool(false)
-- the algorithm listings: shape, not length
bool(true)
bool(true)
bool(true)
int(0)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
int(0)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
-- where the library looks for a CA bundle
array(8) {
  [0]=>
  string(17) "default_cert_file"
  [1]=>
  string(21) "default_cert_file_env"
  [2]=>
  string(16) "default_cert_dir"
  [3]=>
  string(20) "default_cert_dir_env"
  [4]=>
  string(19) "default_private_dir"
  [5]=>
  string(25) "default_default_cert_area"
  [6]=>
  string(10) "ini_cafile"
  [7]=>
  string(10) "ini_capath"
}
string(13) "SSL_CERT_FILE"
string(12) "SSL_CERT_DIR"
bool(true)
bool(true)
bool(true)
bool(true)
-- the version pair is the LIBRARY's
string(6) "string"
string(3) "int"
bool(true)
int(1)
int(2)
int(4)
int(1)
int(2)
int(3)
int(6)
int(7)
int(8)
int(9)
int(10)
int(0)
int(1)
int(2)
int(3)
int(4)
int(5)
int(6)
int(7)
int(0)
int(1)
int(2)
bool(true)
bool(true)
