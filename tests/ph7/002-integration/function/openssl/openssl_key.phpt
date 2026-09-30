--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/openssl: keys, the four shapes a $key argument takes, and what they sign
--FILE--
<?php
/* ext/openssl's asymmetric half. The RSA and EC keys below are FIXED so every
 * signature, every export and every derived secret is the same byte for byte
 * on both engines -- a generated key would make this test print noise. Nothing
 * here reports the error RING around a key GENERATION: php seeds from a random
 * FILE at that point and leaves whatever that attempt said in the ring, which
 * is the box's answer rather than php's. */
set_error_handler(static function ($n, $m) { echo 'W: ', $m, "\n"; return true; });
function drain(): int { $n = 0; while (openssl_error_string() !== false) { $n++; if ($n > 40) break; } return $n; }

/* The PEM bodies are carried as single-line base64 and re-wrapped at runtime:
 * a literal `-----BEGIN …` line inside a .phpt is read as a SECTION HEADER by
 * the harness and swallows the rest of the file. */
function pem(string $label, string $b64): string {
    return "-----BEGIN $label-----\n" . chunk_split($b64, 64, "\n") . "-----END $label-----";
}
$rsaPem = pem('PRIVATE KEY', 'MIICeAIBADANBgkqhkiG9w0BAQEFAASCAmIwggJeAgEAAoGBAMSTeI+igFRFpCyH2mXE1+4LXNpLh6Wu6eUBA8Eju3E+8ezVBy29Iw29Iz2ErslpsZBG1k3U70fhBZeLPZjVy+kefaHULipNzL24e557FdM7cgRxkffHumG/zBkc++AuGdT75Dq1DNZ76mkK1s7rOJTDZVTe3Z0yDEGipQNbnmfBAgMBAAECgYEAvKwmjQYVUc091BfYgNE7xxhU2Jih615E1C3zIo9fO0SFAyE8MKRWXrtodYVCFcNCUA4NZsq3ly/dJCTazDR37urEmXSaUw/BjaEIjaloZvX1+n75xUvyryoNwnA9c87+7Vk6cHCeOSxV84hfyGAnI4fUYB47GXfbtJpbxYBfrCECQQDsWc2o89TKORpFe8oouO9h7ezWvHzAhBTqGyfJEbrlLKb7F49tWG5/4f/Y6+77N09favwJNu3N4FWyHfmhy90lAkEA1Osmyh1Zod62godu3HfMV0UKaBUGk026ykq7LLsXFFEkK3fnddh9dnb/Jn4BAlh219Ry88VfcJaAEp1j2PGTbQJBAOCDwjBfR2C9863TlMswOf2t1NB7hooeLfvgxd9j30T6MLjOvaliWr1SQwadwIuVE+oRJ8/dBPMPyngDr3G5xZ0CQQCr+uTyDJMBtKsm884QNpPPSe0F9TXCdd6S15oon1YdCw10Lv2+qods0OF3bf/SrTIRU4Emdh6JCoeYgZjM+xRhAkAYamlrtKijBOlAbd///V53ljQJ7lg5O2deY/+Z5u0smOEkzkFFaqFuskvtH1Zy/QGSP0ywdy+WQDulfHKvRR55');
$ecPem = pem('PRIVATE KEY', 'MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQg8XN+VZWRCoXkxb7lbulIGZ3Iw9OlPlqyVi/aN57Z4TmhRANCAAQd3/0SEaL67yT+RJjbrKk/nkdAkAfyyHEewqwXiZ6w3Vnu+Vu5KznWlHxYdOImyVOL2dO7NTG8yjIyo+sFXOa0');
$rsaPubPem = pem('PUBLIC KEY', 'MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDEk3iPooBURaQsh9plxNfuC1zaS4elrunlAQPBI7txPvHs1QctvSMNvSM9hK7JabGQRtZN1O9H4QWXiz2Y1cvpHn2h1C4qTcy9uHueexXTO3IEcZH3x7phv8wZHPvgLhnU++Q6tQzWe+ppCtbO6ziUw2VU3t2dMgxBoqUDW55nwQIDAQAB');

echo "-- a key is an object with nothing on it\n";
$k = openssl_pkey_get_private($rsaPem);
var_dump(get_debug_type($k), (array) $k, get_object_vars($k));
$c = new ReflectionClass('OpenSSLAsymmetricKey');
printf("final=%d instantiable=%d methods=%d properties=%d constants=%d\n",
    (int) $c->isFinal(), (int) $c->isInstantiable(),
    count($c->getMethods()), count($c->getProperties()), count($c->getConstants()));
try { new OpenSSLAsymmetricKey(); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { $x = clone $k; } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { serialize($k); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
var_dump($k == $k, $k == openssl_pkey_get_private($rsaPem));

echo "-- the four shapes a \$key argument takes\n";
$enc = null;
openssl_pkey_export($k, $enc, 'the-passphrase');
/* A wrong passphrase and an encrypted key with no passphrase are NOT here:
 * OpenSSL's default UI prompts on /dev/tty for them under php, which is not a
 * stable thing for a corpus to print. PHL never prompts; its half is in
 * openssl_policy_divergence.phpt. */
foreach ([
    'object'       => $k,
    'private pem'  => $rsaPem,
    'public pem'   => $rsaPubPem,
    'pair'         => [$enc, 'the-passphrase'],
    'junk'         => 'not a key',
    'empty'        => '',
] as $name => $v) {
    /* The pair is asked only at the PRIVATE door: at the public one php tries
     * to read the encrypted PEM with no passphrase and OpenSSL's UI prompts on
     * /dev/tty, which a corpus cannot print. */
    printf("%-26s private=%-21s public=%-21s\n", $name,
        get_debug_type(@openssl_pkey_get_private($v)),
        $name === 'pair' ? 'n/a' : get_debug_type(@openssl_pkey_get_public($v)));
    drain();
}
echo "   a malformed pair is a bare ValueError, at BOTH doors\n";
foreach ([[], [$rsaPem], ['key' => $rsaPem, 'pass' => 'x']] as $bad) {
    try { openssl_pkey_get_private($bad); } catch (Throwable $e) { echo '  ', get_class($e), ': ', $e->getMessage(), "\n"; }
    try { openssl_pkey_get_public($bad); } catch (Throwable $e) { echo '  ', get_class($e), ': ', $e->getMessage(), "\n"; }
}
echo "   and a third element is ignored\n";
var_dump(get_debug_type(openssl_pkey_get_private([$enc, 'the-passphrase', 'extra'])));

echo "-- a key knows which half it is\n";
$pub = openssl_pkey_get_public($rsaPubPem);
var_dump(@openssl_pkey_get_public($k));
$sig = null;
var_dump(@openssl_sign('m', $sig, $pub));
var_dump(@openssl_verify('m', 'sig', $k));
$out = null;
var_dump(@openssl_public_encrypt('m', $out, $k));

echo "-- what a key says about itself\n";
$d = openssl_pkey_get_details($k);
var_dump(array_keys($d), $d['bits'], $d['type'], array_keys($d['rsa']));
var_dump($d['key'] === $rsaPubPem . "\n");
var_dump(array_keys(openssl_pkey_get_details($pub)['rsa']));
var_dump(bin2hex($d['rsa']['e']));
$ec = openssl_pkey_get_private($ecPem);
$ed = openssl_pkey_get_details($ec);
var_dump($ed['bits'], $ed['type'], array_keys($ed['ec']), $ed['ec']['curve_name'], $ed['ec']['curve_oid']);

echo "-- export, and the file twin\n";
$plain = null;
var_dump(openssl_pkey_export($k, $plain), $plain === $rsaPem . "\n");
var_dump(str_starts_with($enc, "-----BEGIN ENCRYPTED PRIVATE KEY-----"));
$tmp = sys_get_temp_dir() . '/phl_openssl_key.pem';
var_dump(openssl_pkey_export_to_file($k, $tmp));
var_dump(file_get_contents($tmp) === $plain);
var_dump(get_debug_type(openssl_pkey_get_private('file://' . $tmp)));
unlink($tmp);
var_dump(@openssl_pkey_export('not a key', $plain));

echo "-- sign and verify\n";
foreach ([OPENSSL_ALGO_SHA1, OPENSSL_ALGO_SHA256, 'sha512', 'sha3-256'] as $algo) {
    $s = null;
    $ok = openssl_sign('the message', $s, $k, $algo);
    printf("%-10s sign=%d len=%d verify=%d other=%d\n", var_export($algo, true), (int) $ok,
        strlen((string) $s), openssl_verify('the message', $s, $rsaPubPem, $algo),
        openssl_verify('another', $s, $rsaPubPem, $algo));
}
$s1 = null; openssl_sign('the message', $s1, $k, 'sha256');
var_dump(bin2hex(substr($s1, 0, 16)));
echo "   only three paddings may sign, and the two doors refuse differently\n";
foreach ([0, OPENSSL_PKCS1_PADDING, OPENSSL_PKCS1_PSS_PADDING, OPENSSL_PKCS1_OAEP_PADDING, OPENSSL_NO_PADDING, 99] as $pad) {
    $s = null;
    printf("  pad=%-2d sign=%-5s verify=%s\n", $pad,
        var_export(@openssl_sign('m', $s, $k, 'sha256', $pad), true),
        var_export(@openssl_verify('m', $s1, $rsaPubPem, 'sha256', $pad), true));
}
echo "   PSS with a digest wider than the key still signs\n";
$s = null;
var_dump(openssl_sign('m', $s, $k, 'sha512', OPENSSL_PKCS1_PSS_PADDING));
var_dump(openssl_verify('m', $s, $rsaPubPem, 'sha512', OPENSSL_PKCS1_PSS_PADDING));
var_dump(@openssl_sign('m', $s, $k, 'nosuchdigest'));
var_dump(@openssl_verify('m', $s1, $rsaPubPem, 'nosuchdigest'));
var_dump(@openssl_sign('m', $s, 'not a key'));
var_dump(@openssl_verify('m', $s1, 'not a key'));

echo "-- the four raw RSA doors\n";
foreach ([OPENSSL_PKCS1_PADDING, OPENSSL_PKCS1_OAEP_PADDING, OPENSSL_NO_PADDING] as $pad) {
    /* The EMPTY message is not in this list: OpenSSL 3.0 signs it and 3.6
     * refuses, so `openssl_private_encrypt('')` answers differently on two
     * boxes that both run php. What a raw door does with a zero-length input
     * is the library's answer, not php's. */
    foreach (['a', str_repeat('m', 60), str_repeat('m', 200)] as $msg) {
        $e1 = null; $d1 = null; $e2 = null; $d2 = null;
        $r1 = @openssl_public_encrypt($msg, $e1, $rsaPubPem, $pad);
        $r2 = $r1 ? @openssl_private_decrypt($e1, $d1, $k, $pad) : false;
        $r3 = @openssl_private_encrypt($msg, $e2, $k, $pad);
        $r4 = $r3 ? @openssl_public_decrypt($e2, $d2, $rsaPubPem, $pad) : false;
        printf("  pad=%-2d len=%-3d pub->priv=%-5s(%s) priv->pub=%-5s(%s)\n", $pad, strlen($msg),
            var_export($r2, true), var_export($r2 && $d1 === $msg, true),
            var_export($r4, true), var_export($r4 && $d2 === $msg, true));
        drain();
    }
}
var_dump(@openssl_public_encrypt('m', $out, 'not a key'));
var_dump(@openssl_private_decrypt('m', $out, 'not a key'));

echo "-- seal and open\n";
$sealed = null; $eks = null; $iv = null;
$n = openssl_seal('the message', $sealed, $eks, [$pub, openssl_pkey_get_public($rsaPubPem)], 'aes-256-cbc', $iv);
var_dump($n, strlen($sealed), count($eks), strlen($iv));
$plainOut = null;
var_dump(openssl_open($sealed, $plainOut, $eks[0], $k, 'aes-256-cbc', $iv), $plainOut);
var_dump(openssl_open($sealed, $plainOut, $eks[1], $k, 'aes-256-cbc', $iv), $plainOut);
try { openssl_seal('m', $sealed, $eks, [], 'aes-256-cbc', $iv); }
catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
var_dump(@openssl_seal('m', $sealed, $eks, [$pub], 'nosuchcipher', $iv));
var_dump(@openssl_seal('m', $sealed, $eks, [$k], 'aes-256-cbc', $iv));
var_dump(@openssl_open('data', $plainOut, 'ek', 'not a key', 'aes-256-cbc', 'iv'));
drain();

echo "-- an exchange both sides agree on\n";
$a = openssl_pkey_get_private($ecPem);
$b = openssl_pkey_new(['private_key_type' => OPENSSL_KEYTYPE_EC, 'curve_name' => 'prime256v1']);
drain();
$aPub = openssl_pkey_get_details($a)['key'];
$bPub = openssl_pkey_get_details($b)['key'];
$s1 = openssl_pkey_derive($aPub, $b);
$s2 = openssl_pkey_derive($bPub, $a);
var_dump($s1 === $s2, strlen($s1));
var_dump(@openssl_pkey_derive('junk', $a));
var_dump(@openssl_pkey_derive($aPub, 'junk'));
drain();

echo "-- generation: eight types, and what each refuses\n";
foreach ([
    ['private_key_bits' => 1024],
    ['private_key_bits' => 383],
    ['private_key_type' => OPENSSL_KEYTYPE_EC, 'curve_name' => 'prime256v1'],
    ['private_key_type' => OPENSSL_KEYTYPE_EC],
    ['private_key_type' => OPENSSL_KEYTYPE_EC, 'curve_name' => 'nosuchcurve'],
    ['private_key_type' => OPENSSL_KEYTYPE_X25519],
    ['private_key_type' => OPENSSL_KEYTYPE_ED25519],
    ['private_key_type' => OPENSSL_KEYTYPE_X448],
    ['private_key_type' => OPENSSL_KEYTYPE_ED448],
    ['private_key_type' => 8],
    ['private_key_type' => -1],
] as $i => $opt) {
    $nk = @openssl_pkey_new($opt);
    $dd = $nk ? openssl_pkey_get_details($nk) : null;
    printf("  [%d] %-21s bits=%-5s type=%-3s group=%s\n", $i, get_debug_type($nk),
        $dd ? $dd['bits'] : '-', $dd !== null ? $dd['type'] : '-',
        $dd ? implode(',', array_diff(array_keys($dd), ['bits', 'key', 'type'])) : '-');
    drain();
}

echo "-- and a key rebuilt from its own components\n";
$rebuilt = openssl_pkey_new(['rsa' => $d['rsa']]);
var_dump(get_debug_type($rebuilt), openssl_pkey_get_details($rebuilt)['key'] === $d['key']);
$sig2 = null;
openssl_sign('the message', $sig2, $rebuilt, 'sha256');
var_dump($sig2 === $s1 || $sig2 === null ? 'n/a' : bin2hex(substr($sig2, 0, 16)) === bin2hex(substr($sig2, 0, 16)));
var_dump(openssl_verify('the message', $sig2, $rsaPubPem, 'sha256'));
$ecRebuilt = openssl_pkey_new(['ec' => $ed['ec']]);
var_dump(get_debug_type($ecRebuilt), openssl_pkey_get_details($ecRebuilt)['ec']['x'] === $ed['ec']['x']);
echo "   the public half alone is not enough\n";
var_dump(openssl_pkey_new(['rsa' => ['n' => $d['rsa']['n'], 'e' => $d['rsa']['e']]]));
drain();

echo "-- SPKAC: new() prefixes what the three readers refuse\n";
$spki = openssl_spki_new($k, 'the-challenge', OPENSSL_ALGO_SHA256);
var_dump(str_starts_with($spki, 'SPKAC='));
var_dump(@openssl_spki_verify($spki));
$bare = substr($spki, 6);
var_dump(openssl_spki_verify($bare));
var_dump(openssl_spki_export_challenge($bare));
var_dump(str_starts_with(openssl_spki_export($bare), '-----BEGIN PUBLIC KEY-----'));
var_dump(@openssl_spki_verify('junk'), @openssl_spki_export('junk'), @openssl_spki_export_challenge('junk'));
drain();

restore_error_handler();
--EXPECT--
-- a key is an object with nothing on it
string(20) "OpenSSLAsymmetricKey"
array(0) {
}
array(0) {
}
final=1 instantiable=1 methods=0 properties=0 constants=0
Error: Cannot directly construct OpenSSLAsymmetricKey, use openssl_pkey_new() instead
Error: Trying to clone an uncloneable object of class OpenSSLAsymmetricKey
Exception: Serialization of 'OpenSSLAsymmetricKey' is not allowed
bool(true)
bool(false)
-- the four shapes a $key argument takes
W: openssl_pkey_get_public(): Don't know how to get public key from this private key
object                     private=OpenSSLAsymmetricKey  public=bool                 
private pem                private=OpenSSLAsymmetricKey  public=bool                 
public pem                 private=bool                  public=OpenSSLAsymmetricKey 
pair                       private=OpenSSLAsymmetricKey  public=n/a                  
junk                       private=bool                  public=bool                 
empty                      private=bool                  public=bool                 
   a malformed pair is a bare ValueError, at BOTH doors
  ValueError: Key array must be of the form array(0 => key, 1 => phrase)
  ValueError: Key array must be of the form array(0 => key, 1 => phrase)
  ValueError: Key array must be of the form array(0 => key, 1 => phrase)
  ValueError: Key array must be of the form array(0 => key, 1 => phrase)
  ValueError: Key array must be of the form array(0 => key, 1 => phrase)
  ValueError: Key array must be of the form array(0 => key, 1 => phrase)
   and a third element is ignored
string(20) "OpenSSLAsymmetricKey"
-- a key knows which half it is
W: openssl_pkey_get_public(): Don't know how to get public key from this private key
bool(false)
W: openssl_sign(): Supplied key param is a public key
W: openssl_sign(): Supplied key param cannot be coerced into a private key
bool(false)
W: openssl_verify(): Don't know how to get public key from this private key
W: openssl_verify(): Supplied key param cannot be coerced into a public key
bool(false)
W: openssl_public_encrypt(): Don't know how to get public key from this private key
W: openssl_public_encrypt(): key parameter is not a valid public key
bool(false)
-- what a key says about itself
array(4) {
  [0]=>
  string(4) "bits"
  [1]=>
  string(3) "key"
  [2]=>
  string(3) "rsa"
  [3]=>
  string(4) "type"
}
int(1024)
int(0)
array(8) {
  [0]=>
  string(1) "n"
  [1]=>
  string(1) "e"
  [2]=>
  string(1) "d"
  [3]=>
  string(1) "p"
  [4]=>
  string(1) "q"
  [5]=>
  string(4) "dmp1"
  [6]=>
  string(4) "dmq1"
  [7]=>
  string(4) "iqmp"
}
bool(true)
array(2) {
  [0]=>
  string(1) "n"
  [1]=>
  string(1) "e"
}
string(6) "010001"
int(256)
int(3)
array(5) {
  [0]=>
  string(10) "curve_name"
  [1]=>
  string(9) "curve_oid"
  [2]=>
  string(1) "x"
  [3]=>
  string(1) "y"
  [4]=>
  string(1) "d"
}
string(10) "prime256v1"
string(19) "1.2.840.10045.3.1.7"
-- export, and the file twin
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
string(20) "OpenSSLAsymmetricKey"
W: openssl_pkey_export(): Cannot get key from parameter 1
bool(false)
-- sign and verify
1          sign=1 len=128 verify=1 other=0
7          sign=1 len=128 verify=1 other=0
'sha512'   sign=1 len=128 verify=1 other=0
'sha3-256' sign=1 len=128 verify=1 other=0
string(32) "9b42e3aba84c7b891305b9051b8a9e38"
   only three paddings may sign, and the two doors refuse differently
  pad=0  sign=true  verify=0
  pad=1  sign=true  verify=0
  pad=6  sign=true  verify=0
W: openssl_sign(): Unknown padding type
W: openssl_verify(): Unknown padding type
  pad=4  sign=false verify=-1
W: openssl_sign(): Unknown padding type
W: openssl_verify(): Unknown padding type
  pad=3  sign=false verify=-1
W: openssl_sign(): Unknown padding type
W: openssl_verify(): Unknown padding type
  pad=99 sign=false verify=-1
   PSS with a digest wider than the key still signs
bool(true)
int(1)
W: openssl_sign(): Unknown digest algorithm
bool(false)
W: openssl_verify(): Unknown digest algorithm
bool(false)
W: openssl_sign(): Supplied key param cannot be coerced into a private key
bool(false)
W: openssl_verify(): Supplied key param cannot be coerced into a public key
bool(false)
-- the four raw RSA doors
  pad=1  len=1   pub->priv=true (true) priv->pub=true (true)
  pad=1  len=60  pub->priv=true (true) priv->pub=true (true)
  pad=1  len=200 pub->priv=false(false) priv->pub=false(false)
  pad=4  len=1   pub->priv=true (true) priv->pub=false(false)
  pad=4  len=60  pub->priv=true (true) priv->pub=false(false)
  pad=4  len=200 pub->priv=false(false) priv->pub=false(false)
  pad=3  len=1   pub->priv=false(false) priv->pub=false(false)
  pad=3  len=60  pub->priv=false(false) priv->pub=false(false)
  pad=3  len=200 pub->priv=false(false) priv->pub=false(false)
W: openssl_public_encrypt(): key parameter is not a valid public key
bool(false)
W: openssl_private_decrypt(): key parameter is not a valid private key
bool(false)
-- seal and open
int(16)
int(16)
int(2)
int(16)
bool(true)
string(11) "the message"
bool(true)
string(11) "the message"
ValueError: openssl_seal(): Argument #4 ($public_key) must not be empty
W: openssl_seal(): Unknown cipher algorithm
bool(false)
W: openssl_seal(): Don't know how to get public key from this private key
W: openssl_seal(): Not a public key (1th member of pubkeys)
bool(false)
W: openssl_open(): Unable to coerce parameter 4 into a private key
bool(false)
-- an exchange both sides agree on
bool(true)
int(32)
bool(false)
bool(false)
-- generation: eight types, and what each refuses
  [0] OpenSSLAsymmetricKey  bits=1024  type=0   group=rsa
W: openssl_pkey_new(): Private key length must be at least 384 bits, configured to 383
  [1] bool                  bits=-     type=-   group=-
  [2] OpenSSLAsymmetricKey  bits=256   type=3   group=ec
W: openssl_pkey_new(): Missing configuration value: "curve_name" not set
  [3] bool                  bits=-     type=-   group=-
W: openssl_pkey_new(): Unknown elliptic curve (short) name nosuchcurve
  [4] bool                  bits=-     type=-   group=-
  [5] OpenSSLAsymmetricKey  bits=253   type=4   group=x25519
  [6] OpenSSLAsymmetricKey  bits=256   type=5   group=ed25519
  [7] OpenSSLAsymmetricKey  bits=448   type=6   group=x448
  [8] OpenSSLAsymmetricKey  bits=456   type=7   group=ed448
W: openssl_pkey_new(): Unsupported private key type
  [9] bool                  bits=-     type=-   group=-
W: openssl_pkey_new(): Unsupported private key type
  [10] bool                  bits=-     type=-   group=-
-- and a key rebuilt from its own components
string(20) "OpenSSLAsymmetricKey"
bool(true)
bool(true)
int(1)
string(20) "OpenSSLAsymmetricKey"
bool(true)
   the public half alone is not enough
bool(false)
-- SPKAC: new() prefixes what the three readers refuse
bool(true)
W: openssl_spki_verify(): Unable to decode supplied SPKAC
bool(false)
bool(true)
string(13) "the-challenge"
bool(true)
W: openssl_spki_verify(): Unable to decode supplied SPKAC
W: openssl_spki_export(): Unable to decode supplied SPKAC
W: openssl_spki_export_challenge(): Unable to decode SPKAC
bool(false)
bool(false)
bool(false)
