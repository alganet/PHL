--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/phar: the three OpenSSL signatures, written, verified and refused
--DESCRIPTION--
php's `Phar::getSupportedSignatures()` answers for the BUILD, and it grows by
three the moment that build links crypto. An OpenSSL-signed archive is the one
kind whose public half is NOT inside it: php reads a `<archive>.pubkey` beside
it, and refuses the archive when there is none rather than trusting it.
--INI--
phar.readonly=0
--SKIPIF--
<?php
/* php on Windows names one archive by the 8.3 short temp path in some
 * sentences and the long one, either slash, in others */
if (PHP_OS_FAMILY === 'Windows') {
    die("skip the archive paths pinned here are POSIX's\n");
}
--FILE--
<?php
/* ext/phar's OpenSSL signatures -- the three flavours php grows the moment a
 * build links crypto. The private key is FIXED so the run is deterministic;
 * the archives are not compared BYTE for byte between engines, because a
 * manifest stamps every entry with the current time and two runs a second
 * apart differ there whatever signs them. What is pinned is the round trip,
 * the two refusals and the sentence a tampered archive answers with. */
error_reporting(E_ALL);
$dir = sys_get_temp_dir() . '/phlt-pharssl-' . getmypid();
@mkdir($dir);
function pem(string $label, string $b64): string {
    return "-----BEGIN $label-----\n" . chunk_split($b64, 64, "\n") . "-----END $label-----";
}
$priv = pem('PRIVATE KEY', 'MIICeAIBADANBgkqhkiG9w0BAQEFAASCAmIwggJeAgEAAoGBAMSTeI+igFRFpCyH2mXE1+4LXNpLh6Wu6eUBA8Eju3E+8ezVBy29Iw29Iz2ErslpsZBG1k3U70fhBZeLPZjVy+kefaHULipNzL24e557FdM7cgRxkffHumG/zBkc++AuGdT75Dq1DNZ76mkK1s7rOJTDZVTe3Z0yDEGipQNbnmfBAgMBAAECgYEAvKwmjQYVUc091BfYgNE7xxhU2Jih615E1C3zIo9fO0SFAyE8MKRWXrtodYVCFcNCUA4NZsq3ly/dJCTazDR37urEmXSaUw/BjaEIjaloZvX1+n75xUvyryoNwnA9c87+7Vk6cHCeOSxV84hfyGAnI4fUYB47GXfbtJpbxYBfrCECQQDsWc2o89TKORpFe8oouO9h7ezWvHzAhBTqGyfJEbrlLKb7F49tWG5/4f/Y6+77N09favwJNu3N4FWyHfmhy90lAkEA1Osmyh1Zod62godu3HfMV0UKaBUGk026ykq7LLsXFFEkK3fnddh9dnb/Jn4BAlh219Ry88VfcJaAEp1j2PGTbQJBAOCDwjBfR2C9863TlMswOf2t1NB7hooeLfvgxd9j30T6MLjOvaliWr1SQwadwIuVE+oRJ8/dBPMPyngDr3G5xZ0CQQCr+uTyDJMBtKsm884QNpPPSe0F9TXCdd6S15oon1YdCw10Lv2+qods0OF3bf/SrTIRU4Emdh6JCoeYgZjM+xRhAkAYamlrtKijBOlAbd///V53ljQJ7lg5O2deY/+Z5u0smOEkzkFFaqFuskvtH1Zy/QGSP0ywdy+WQDulfHKvRR55');
$key = openssl_pkey_get_private($priv);
$pub = openssl_pkey_get_details($key)['key'];
/* a message may name the directory with symlinks resolved (macOS's /private/var) */
$clean = static function (string $s) use ($dir): string {
    return str_replace([(realpath($dir) ?: $dir) . '/', $dir . '/'], '', $s);
};

echo "-- the build reports what it can sign with\n";
$sig = Phar::getSupportedSignatures();
var_dump(in_array('OpenSSL', $sig, true), in_array('OpenSSL_SHA256', $sig, true),
         in_array('OpenSSL_SHA512', $sig, true));
var_dump(Phar::OPENSSL, Phar::OPENSSL_SHA256, Phar::OPENSSL_SHA512);

echo "-- one archive per flavour, written and read back\n";
foreach ([Phar::OPENSSL => 'OpenSSL', Phar::OPENSSL_SHA256 => 'OpenSSL_SHA256',
          Phar::OPENSSL_SHA512 => 'OpenSSL_SHA512'] as $algo => $name) {
    $f = "$dir/s$algo.phar";
    @unlink($f); @unlink("$f.pubkey");
    $p = new Phar($f);
    $p->addFromString('greet.php', '<?php return "hello from ' . $name . '";');
    $p->setStub('<?php __HALT_COMPILER();');
    $p->setSignatureAlgorithm($algo, $priv);
    unset($p);
    file_put_contents("$f.pubkey", $pub);
    $q = new Phar($f);
    $s = $q->getSignature();
    printf("  %-16s type=%-16s hexlen=%d verified=%d\n", "algo=$algo", $s['hash_type'],
        strlen($s['hash']), 1);
    printf("  %s\n", include "phar://$f/greet.php");
    unset($q);
}

echo "-- the public half is a FILE beside the archive, and it is required\n";
$f = "$dir/s16.phar";
copy($f, "$dir/nokey.phar");
try { new Phar("$dir/nokey.phar"); }
catch (Throwable $e) { echo '  ', get_class($e), ': ', $clean($e->getMessage()), "\n"; }

echo "-- a tampered archive is refused with its own sentence\n";
$bytes = file_get_contents($f);
$bytes[strlen($bytes) - 200] = chr(ord($bytes[strlen($bytes) - 200]) ^ 0x01);
file_put_contents("$dir/tampered.phar", $bytes);
copy("$f.pubkey", "$dir/tampered.phar.pubkey");
try { new Phar("$dir/tampered.phar"); }
catch (Throwable $e) { echo '  ', get_class($e), ': ', $clean($e->getMessage()), "\n"; }

echo "-- and the two ways to ask for one without a usable key\n";
@unlink("$dir/nk.phar");
try { $p = new Phar("$dir/nk.phar"); $p->addFromString('a', 'b'); $p->setSignatureAlgorithm(Phar::OPENSSL); }
catch (Throwable $e) { echo '  ', get_class($e), ': ', $clean($e->getMessage()), "\n"; }
unset($p);
@unlink("$dir/bk.phar");
try { $p = new Phar("$dir/bk.phar"); $p->addFromString('a', 'b'); $p->setSignatureAlgorithm(Phar::OPENSSL, 'not a key'); }
catch (Throwable $e) { echo '  ', get_class($e), ': ', $clean($e->getMessage()), "\n"; }
unset($p);
echo "-- a number outside the set is still unknown\n";
@unlink("$dir/un.phar");
try { $p = new Phar("$dir/un.phar"); $p->addFromString('a', 'b'); $p->setSignatureAlgorithm(99); }
catch (Throwable $e) { echo '  ', get_class($e), ': ', $clean($e->getMessage()), "\n"; }
unset($p);

echo "-- a hash-signed archive still says what it always said\n";
@unlink("$dir/h.phar");
$p = new Phar("$dir/h.phar");
$p->addFromString('a.php', '<?php echo 1;');
$p->setStub('<?php __HALT_COMPILER();');
$p->setSignatureAlgorithm(Phar::SHA512);
unset($p);
$bytes = file_get_contents("$dir/h.phar");
$bytes[strlen($bytes) - 20] = chr(ord($bytes[strlen($bytes) - 20]) ^ 0x01);
file_put_contents("$dir/hbad.phar", $bytes);
try { new Phar("$dir/hbad.phar"); }
catch (Throwable $e) { echo '  ', get_class($e), ': ', $clean($e->getMessage()), "\n"; }

foreach (glob($dir . '/*') as $x) { @unlink($x); }
@rmdir($dir);
--EXPECT--
-- the build reports what it can sign with
bool(true)
bool(true)
bool(true)
int(16)
int(17)
int(18)
-- one archive per flavour, written and read back
  algo=16          type=OpenSSL          hexlen=256 verified=1
  hello from OpenSSL
  algo=17          type=OpenSSL_SHA256   hexlen=256 verified=1
  hello from OpenSSL_SHA256
  algo=18          type=OpenSSL_SHA512   hexlen=256 verified=1
  hello from OpenSSL_SHA512
-- the public half is a FILE beside the archive, and it is required
  UnexpectedValueException: phar "nokey.phar" openssl signature could not be verified: openssl public key could not be read
-- a tampered archive is refused with its own sentence
  UnexpectedValueException: phar "tampered.phar" openssl signature could not be verified: broken openssl signature
-- and the two ways to ask for one without a usable key
  PharException: phar error: unable to write signature: unable to write to phar "nk.phar" with requested openssl signature
  PharException: phar error: unable to write signature: unable to process private key
-- a number outside the set is still unknown
  UnexpectedValueException: Unknown signature algorithm specified
-- a hash-signed archive still says what it always said
  UnexpectedValueException: phar "hbad.phar" SHA512 signature could not be verified: broken signature
