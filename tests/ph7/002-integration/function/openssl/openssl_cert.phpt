--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/openssl: certificates, signing requests, PKCS#12 bundles and S/MIME
--FILE--
<?php
/* ext/openssl's certificate half. The RSA key is FIXED so the subject, the
 * serial and the parse are the same on both engines. What is NOT pinned here
 * is anything the SYSTEM openssl.cnf decides -- the extra distinguished-name
 * fields php fills in from `<name>_default`, and the extensions
 * openssl_csr_sign() adds from `x509_extensions` -- because those are the
 * box's configuration rather than php's contract. The test asserts that the
 * two engines see the SAME configuration, which is what a user cares about.
 */
set_error_handler(static function ($n, $m) { echo 'W: ', $m, "\n"; return true; });
function drain(): int { $n = 0; while (openssl_error_string() !== false) { $n++; if ($n > 40) break; } return $n; }
function pem(string $label, string $b64): string {
    return "-----BEGIN $label-----\n" . chunk_split($b64, 64, "\n") . "-----END $label-----";
}
$rsaPem = pem('PRIVATE KEY', 'MIICeAIBADANBgkqhkiG9w0BAQEFAASCAmIwggJeAgEAAoGBAMSTeI+igFRFpCyH2mXE1+4LXNpLh6Wu6eUBA8Eju3E+8ezVBy29Iw29Iz2ErslpsZBG1k3U70fhBZeLPZjVy+kefaHULipNzL24e557FdM7cgRxkffHumG/zBkc++AuGdT75Dq1DNZ76mkK1s7rOJTDZVTe3Z0yDEGipQNbnmfBAgMBAAECgYEAvKwmjQYVUc091BfYgNE7xxhU2Jih615E1C3zIo9fO0SFAyE8MKRWXrtodYVCFcNCUA4NZsq3ly/dJCTazDR37urEmXSaUw/BjaEIjaloZvX1+n75xUvyryoNwnA9c87+7Vk6cHCeOSxV84hfyGAnI4fUYB47GXfbtJpbxYBfrCECQQDsWc2o89TKORpFe8oouO9h7ezWvHzAhBTqGyfJEbrlLKb7F49tWG5/4f/Y6+77N09favwJNu3N4FWyHfmhy90lAkEA1Osmyh1Zod62godu3HfMV0UKaBUGk026ykq7LLsXFFEkK3fnddh9dnb/Jn4BAlh219Ry88VfcJaAEp1j2PGTbQJBAOCDwjBfR2C9863TlMswOf2t1NB7hooeLfvgxd9j30T6MLjOvaliWr1SQwadwIuVE+oRJ8/dBPMPyngDr3G5xZ0CQQCr+uTyDJMBtKsm884QNpPPSe0F9TXCdd6S15oon1YdCw10Lv2+qods0OF3bf/SrTIRU4Emdh6JCoeYgZjM+xRhAkAYamlrtKijBOlAbd///V53ljQJ7lg5O2deY/+Z5u0smOEkzkFFaqFuskvtH1Zy/QGSP0ywdy+WQDulfHKvRR55');
$k = openssl_pkey_get_private($rsaPem);
$pubPem = openssl_pkey_get_details($k)['key'];

echo "-- a request, its subject and the config behind it\n";
$dn = ['countryName' => 'BR', 'organizationName' => 'PHL', 'commonName' => 'phl.test',
       'emailAddress' => 'dev@phl.test'];
$csr = openssl_csr_new($dn, $k, ['digest_alg' => 'sha256']);
var_dump(get_debug_type($csr));
$short = openssl_csr_get_subject($csr);
$long = openssl_csr_get_subject($csr, false);
echo "   the caller's fields come FIRST, in their own order\n";
var_dump(array_slice($short, 0, 4, true));
var_dump(array_slice(array_keys($long), 0, 4));
echo "   anything after them is the config's own default, and how MANY of those\n";
echo "   there are is the box's openssl.cnf rather than php's contract\n";
var_dump(count($short) === count($long));
var_dump(array_intersect(array_slice(array_keys($long), 4), array_keys($dn)) === []);
echo "   a name no object table knows is skipped, not fatal\n";
$csr2 = openssl_csr_new(['nosuchfield' => 'v', 'commonName' => 'skip.test'], $k, ['digest_alg' => 'sha256']);
var_dump(get_debug_type($csr2), openssl_csr_get_subject($csr2)['CN']);
var_dump(drain() > 0);
var_dump(get_debug_type(openssl_csr_get_public_key($csr)));

echo "-- export and re-import\n";
$csrPem = null;
var_dump(openssl_csr_export($csr, $csrPem));
var_dump(str_contains($csrPem, 'BEGIN CERTIFICATE REQUEST'), substr_count($csrPem, "\n") < 20);
$withText = null;
openssl_csr_export($csr, $withText, false);
var_dump(substr_count($withText, "\n") > 20, str_contains($withText, 'Certificate Request:'));
var_dump(openssl_csr_get_subject($csrPem) === $short);
$tmp = sys_get_temp_dir() . '/phl_openssl.csr';
var_dump(openssl_csr_export_to_file($csr, $tmp), file_get_contents($tmp) === $csrPem);
unlink($tmp);

echo "-- signing it\n";
$cert = openssl_csr_sign($csr, null, $k, 365, ['digest_alg' => 'sha256'], 4242);
var_dump(get_debug_type($cert));
$p = openssl_x509_parse($cert);
var_dump(array_keys($p));
var_dump($p['subject'] === $short, $p['issuer'] === $short);
var_dump($p['version'], $p['serialNumber'], $p['serialNumberHex']);
var_dump($p['signatureTypeSN'], $p['signatureTypeLN'], $p['signatureTypeNID']);
var_dump(strlen($p['hash']), strlen($p['validFrom']), strlen($p['validTo']));
var_dump($p['validTo_time_t'] - $p['validFrom_time_t']);
var_dump(str_starts_with($p['name'], '/C=BR/O=PHL/CN=phl.test/emailAddress=dev@phl.test'));
echo "   purposes are keyed by php's own X509_PURPOSE_* number\n";
var_dump(array_keys($p['purposes']) === range(1, count($p['purposes'])));
var_dump($p['purposes'][X509_PURPOSE_SSL_SERVER][2], count($p['purposes'][1]));
var_dump(array_slice(array_keys(openssl_x509_parse($cert, false)['subject']), 0, 4));
echo "   a hex serial is the only way to say one that does not fit an int\n";
$big = openssl_csr_sign($csr, null, $k, 1, ['digest_alg' => 'sha256'], 0, '1F2E3D4C5B6A79887766554433221100');
var_dump(openssl_x509_parse($big)['serialNumberHex']);
var_dump(openssl_x509_parse($big)['serialNumber']);

echo "-- what a certificate answers\n";
$certPem = null;
var_dump(openssl_x509_export($cert, $certPem), str_contains($certPem, 'BEGIN CERTIFICATE'));
var_dump(get_debug_type(openssl_x509_read($certPem)));
var_dump(openssl_x509_parse($certPem)['serialNumber']);
var_dump(strlen(openssl_x509_fingerprint($cert)), strlen(openssl_x509_fingerprint($cert, 'sha256')),
         strlen(openssl_x509_fingerprint($cert, 'sha256', true)));
var_dump(openssl_x509_fingerprint($cert) === openssl_x509_fingerprint($certPem));
var_dump(openssl_x509_check_private_key($cert, $k));
var_dump(openssl_x509_check_private_key($cert, openssl_pkey_new(['private_key_bits' => 1024])));
drain();
var_dump(openssl_x509_verify($cert, $pubPem), openssl_x509_verify($cert, $cert));
echo "   a self-signed certificate is trusted by nothing\n";
var_dump(openssl_x509_checkpurpose($cert, X509_PURPOSE_SSL_SERVER, []));
drain();
echo "   and a certificate IS a public key, at every door that wants one\n";
$sig = null;
openssl_sign('the message', $sig, $k, 'sha256');
var_dump(openssl_verify('the message', $sig, $cert, 'sha256'));
var_dump(openssl_verify('the message', $sig, $certPem, 'sha256'));

echo "-- a PKCS#12 bundle\n";
$p12 = null;
var_dump(openssl_pkcs12_export($cert, $p12, $k, 'the-passphrase'), strlen($p12) > 0);
$out = null;
var_dump(openssl_pkcs12_read($p12, $out, 'the-passphrase'), array_keys($out));
var_dump(str_contains($out['cert'], 'BEGIN CERTIFICATE'), str_contains($out['pkey'], 'PRIVATE KEY'));
var_dump(openssl_x509_parse($out['cert'])['serialNumber']);
var_dump(openssl_pkcs12_read($p12, $out2, 'wrong'), $out2);
drain();
$p12b = null;
var_dump(openssl_pkcs12_export($cert, $p12b, $k, 'pw', ['friendly_name' => 'phl', 'extracerts' => [$cert]]));
$out3 = null;
var_dump(openssl_pkcs12_read($p12b, $out3, 'pw'), array_keys($out3), count($out3['extracerts']));
$tmp = sys_get_temp_dir() . '/phl_openssl.p12';
var_dump(openssl_pkcs12_export_to_file($cert, $tmp, $k, 'pw'), filesize($tmp) > 0);
unlink($tmp);

echo "-- S/MIME, both spellings\n";
$in = sys_get_temp_dir() . '/phl_openssl_msg.txt';
file_put_contents($in, "hello signed world\n");
foreach ([['pkcs7', 'openssl_pkcs7_sign', 'openssl_pkcs7_verify', 'openssl_pkcs7_encrypt', 'openssl_pkcs7_decrypt', PKCS7_NOVERIFY],
          ['cms',   'openssl_cms_sign',   'openssl_cms_verify',   'openssl_cms_encrypt',   'openssl_cms_decrypt',   OPENSSL_CMS_NOVERIFY]] as $f) {
    [$name, $sign, $verify, $encrypt, $decrypt, $noverify] = $f;
    $signed = sys_get_temp_dir() . "/phl_openssl.$name";
    $enc = sys_get_temp_dir() . "/phl_openssl.$name.enc";
    $dec = sys_get_temp_dir() . "/phl_openssl.$name.dec";
    printf("%-6s sign=%s smime=%d verify=%s strict=%s\n", $name,
        var_export($sign($in, $signed, $cert, $k, []), true),
        (int) str_starts_with(file_get_contents($signed), 'MIME-Version: 1.0'),
        var_export($verify($signed, $noverify), true),
        var_export($verify($signed, 0), true));
    drain();
    printf("%-6s encrypt=%s decrypt=%s same=%s\n", $name,
        var_export($encrypt($in, $enc, $cert, []), true),
        var_export($decrypt($enc, $dec, $cert, $k), true),
        var_export(@file_get_contents($dec) === file_get_contents($in), true));
    drain();
    @unlink($signed); @unlink($enc); @unlink($dec);
}
unlink($in);

echo "-- every refusal, and which of them says so\n";
$silent = ['openssl_x509_parse', 'openssl_csr_get_subject', 'openssl_csr_get_public_key'];
foreach ($silent as $fn) {
    printf("%-28s %s\n", $fn, var_export(@$fn('not a certificate'), true));
    drain();
}
printf("%-28s %s\n", 'openssl_x509_check_private_key', var_export(@openssl_x509_check_private_key('junk', 'junk'), true));
printf("%-28s %s\n", 'openssl_x509_verify', var_export(@openssl_x509_verify('junk', 'junk'), true));
printf("%-28s %s\n", 'openssl_x509_checkpurpose', var_export(@openssl_x509_checkpurpose('junk', 1, []), true));
drain();
$o = null;
var_dump(openssl_x509_read('junk'));
var_dump(openssl_x509_export('junk', $o));
var_dump(openssl_x509_export_to_file('junk', sys_get_temp_dir() . '/phl_x.crt'));
var_dump(openssl_x509_fingerprint('junk'));
echo "   the certificate is read before the digest is looked up\n";
var_dump(openssl_x509_fingerprint('junk', 'nosuchdigest'));
var_dump(openssl_x509_fingerprint($cert, 'nosuchdigest'));
var_dump(openssl_csr_export('junk', $o));
var_dump(openssl_csr_sign('junk', null, $k, 1));
var_dump(openssl_pkcs12_export('junk', $o, $k, 'pw'));
drain();
echo "   the message doors: SIGN says so, ENCRYPT/DECRYPT/VERIFY do not\n";
var_dump(openssl_pkcs7_sign('/nonexistent', '/tmp/phl_x.p7', 'junk', $k, []));
var_dump(openssl_pkcs7_encrypt('/nonexistent', '/tmp/phl_x.p7', $cert, []));
var_dump(openssl_pkcs7_decrypt('/nonexistent', '/tmp/phl_x.out', $cert, $k));
var_dump(openssl_pkcs7_verify('/nonexistent', 0));
var_dump(openssl_cms_verify('/nonexistent', 0));
$c = null;
var_dump(openssl_pkcs7_read('junk', $c), $c);
var_dump(openssl_cms_read('junk', $c), $c);
drain();
echo "   a string holding DER is not a certificate under php either\n";
$der = base64_decode(implode('', array_filter(explode("\n", $certPem),
    static fn ($l) => $l !== '' && !str_starts_with($l, '-----'))));
var_dump(strlen($der) > 0, openssl_x509_read($der));
drain();

echo "-- the two container classes\n";
foreach (['OpenSSLCertificate' => $cert, 'OpenSSLCertificateSigningRequest' => $csr] as $name => $obj) {
    $c = new ReflectionClass($name);
    printf("%-32s final=%d instantiable=%d methods=%d properties=%d constants=%d\n", $name,
        (int) $c->isFinal(), (int) $c->isInstantiable(), count($c->getMethods()),
        count($c->getProperties()), count($c->getConstants()));
    var_dump((array) $obj, get_object_vars($obj));
    try { new $name(); } catch (Throwable $e) { echo '  ', get_class($e), ': ', $e->getMessage(), "\n"; }
    try { $x = clone $obj; } catch (Throwable $e) { echo '  ', get_class($e), ': ', $e->getMessage(), "\n"; }
    try { serialize($obj); } catch (Throwable $e) { echo '  ', get_class($e), ': ', $e->getMessage(), "\n"; }
}
drain();
restore_error_handler();
--EXPECT--
-- a request, its subject and the config behind it
string(32) "OpenSSLCertificateSigningRequest"
   the caller's fields come FIRST, in their own order
array(4) {
  ["C"]=>
  string(2) "BR"
  ["O"]=>
  string(3) "PHL"
  ["CN"]=>
  string(8) "phl.test"
  ["emailAddress"]=>
  string(12) "dev@phl.test"
}
array(4) {
  [0]=>
  string(11) "countryName"
  [1]=>
  string(16) "organizationName"
  [2]=>
  string(10) "commonName"
  [3]=>
  string(12) "emailAddress"
}
   anything after them is the config's own default, and how MANY of those
   there are is the box's openssl.cnf rather than php's contract
bool(true)
bool(true)
   a name no object table knows is skipped, not fatal
W: openssl_csr_new(): dn: nosuchfield is not a recognized name
string(32) "OpenSSLCertificateSigningRequest"
string(9) "skip.test"
bool(true)
string(20) "OpenSSLAsymmetricKey"
-- export and re-import
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
-- signing it
string(18) "OpenSSLCertificate"
array(16) {
  [0]=>
  string(4) "name"
  [1]=>
  string(7) "subject"
  [2]=>
  string(4) "hash"
  [3]=>
  string(6) "issuer"
  [4]=>
  string(7) "version"
  [5]=>
  string(12) "serialNumber"
  [6]=>
  string(15) "serialNumberHex"
  [7]=>
  string(9) "validFrom"
  [8]=>
  string(7) "validTo"
  [9]=>
  string(16) "validFrom_time_t"
  [10]=>
  string(14) "validTo_time_t"
  [11]=>
  string(15) "signatureTypeSN"
  [12]=>
  string(15) "signatureTypeLN"
  [13]=>
  string(16) "signatureTypeNID"
  [14]=>
  string(8) "purposes"
  [15]=>
  string(10) "extensions"
}
bool(true)
bool(true)
int(2)
string(4) "4242"
string(4) "1092"
string(10) "RSA-SHA256"
string(23) "sha256WithRSAEncryption"
int(668)
int(8)
int(13)
int(13)
int(31536000)
bool(true)
   purposes are keyed by php's own X509_PURPOSE_* number
bool(true)
string(9) "sslserver"
int(3)
array(4) {
  [0]=>
  string(11) "countryName"
  [1]=>
  string(16) "organizationName"
  [2]=>
  string(10) "commonName"
  [3]=>
  string(12) "emailAddress"
}
   a hex serial is the only way to say one that does not fit an int
string(32) "1F2E3D4C5B6A79887766554433221100"
string(38) "41446156801443023921242770679089205504"
-- what a certificate answers
bool(true)
bool(true)
string(18) "OpenSSLCertificate"
string(4) "4242"
int(40)
int(64)
int(32)
bool(true)
bool(true)
bool(false)
int(1)
int(1)
   a self-signed certificate is trusted by nothing
bool(false)
   and a certificate IS a public key, at every door that wants one
int(1)
int(1)
-- a PKCS#12 bundle
bool(true)
bool(true)
bool(true)
array(2) {
  [0]=>
  string(4) "cert"
  [1]=>
  string(4) "pkey"
}
bool(true)
bool(true)
string(4) "4242"
bool(false)
NULL
bool(true)
bool(true)
array(3) {
  [0]=>
  string(4) "cert"
  [1]=>
  string(4) "pkey"
  [2]=>
  string(10) "extracerts"
}
int(1)
bool(true)
bool(true)
-- S/MIME, both spellings
pkcs7  sign=true smime=1 verify=true strict=false
pkcs7  encrypt=true decrypt=true same=false
cms    sign=true smime=1 verify=true strict=false
cms    encrypt=true decrypt=true same=false
-- every refusal, and which of them says so
openssl_x509_parse           false
openssl_csr_get_subject      false
openssl_csr_get_public_key   false
openssl_x509_check_private_key false
openssl_x509_verify          -1
openssl_x509_checkpurpose    -1
W: openssl_x509_read(): X.509 Certificate cannot be retrieved
bool(false)
W: openssl_x509_export(): X.509 Certificate cannot be retrieved
bool(false)
W: openssl_x509_export_to_file(): X.509 Certificate cannot be retrieved
bool(false)
W: openssl_x509_fingerprint(): X.509 Certificate cannot be retrieved
bool(false)
   the certificate is read before the digest is looked up
W: openssl_x509_fingerprint(): X.509 Certificate cannot be retrieved
bool(false)
W: openssl_x509_fingerprint(): Unknown digest algorithm
bool(false)
W: openssl_csr_export(): X.509 Certificate Signing Request cannot be retrieved
bool(false)
W: openssl_csr_sign(): X.509 Certificate Signing Request cannot be retrieved
bool(false)
W: openssl_pkcs12_export(): X.509 Certificate cannot be retrieved
bool(false)
   the message doors: SIGN says so, ENCRYPT/DECRYPT/VERIFY do not
W: openssl_pkcs7_sign(): X.509 Certificate cannot be retrieved
bool(false)
bool(false)
bool(false)
int(-1)
bool(false)
bool(false)
NULL
bool(false)
NULL
   a string holding DER is not a certificate under php either
W: openssl_x509_read(): X.509 Certificate cannot be retrieved
bool(true)
bool(false)
-- the two container classes
OpenSSLCertificate               final=1 instantiable=1 methods=0 properties=0 constants=0
array(0) {
}
array(0) {
}
  Error: Cannot directly construct OpenSSLCertificate, use openssl_x509_read() instead
  Error: Trying to clone an uncloneable object of class OpenSSLCertificate
  Exception: Serialization of 'OpenSSLCertificate' is not allowed
OpenSSLCertificateSigningRequest final=1 instantiable=1 methods=0 properties=0 constants=0
array(0) {
}
array(0) {
}
  Error: Cannot directly construct OpenSSLCertificateSigningRequest, use openssl_csr_new() instead
  Error: Trying to clone an uncloneable object of class OpenSSLCertificateSigningRequest
  Exception: Serialization of 'OpenSSLCertificateSigningRequest' is not allowed
