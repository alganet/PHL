--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/openssl: the inventory, every signature and the three handle classes
--SKIPIF--
<?php
/* a capability guard: an OpenSSL with Argon2 (CI's Windows php links one) gives
 * php two more functions than the inventory pinned here, and setup-php sets
 * openssl.cafile there */
if (function_exists('openssl_password_hash')) {
    die("skip this php's OpenSSL carries Argon2, so its inventory is larger\n");
}
--FILE--
<?php
/* The API half of ext/openssl: what the extension IS. Its ANSWERS are the four
 * tests beside this one. Three names are filtered out of every listing below --
 * openssl_x509_free, openssl_pkey_free and openssl_free_key, which php
 * deprecates and the scope policy removes, so their absence is asserted in
 * openssl_policy_divergence.phpt rather than here; and openssl.libctx, whose
 * choice this build does not make. Nothing prints the LIBRARY's version or its
 * algorithm tables: both move with the OpenSSL a build links. */
const GONE = ['openssl_x509_free', 'openssl_pkey_free', 'openssl_free_key'];

echo "-- the extension\n";
var_dump(extension_loaded('openssl'));
$funcs = array_values(array_filter(get_extension_funcs('openssl'),
    static fn ($f) => !in_array($f, GONE, true)));
echo count($funcs), " functions\n";
echo implode("\n", $funcs), "\n";
$ext = new ReflectionExtension('openssl');
echo implode(', ', $ext->getClassNames()), "\n";
foreach ($ext->getConstants() as $name => $value) {
    if ($name === 'OPENSSL_VERSION_TEXT' || $name === 'OPENSSL_VERSION_NUMBER') {
        printf("%-32s %s\n", $name, get_debug_type($value));
        continue;
    }
    printf("%-32s %s\n", $name, var_export($value, true));
}
foreach ($ext->getINIEntries() as $name => $value) {
    if ($name === 'openssl.libctx') {
        continue;
    }
    printf("%-32s %s\n", $name, var_export($value, true));
}
foreach (['openssl.cafile', 'openssl.capath'] as $ini) {
    $d = ini_get_all('openssl')[$ini];
    printf("%-32s access=%d global=%s\n", $ini, $d['access'], var_export($d['global_value'], true));
}

echo "-- every function's signature\n";
foreach ($funcs as $name) {
    if ($name === 'openssl_pkey_derive') {
        continue;   /* php's third parameter is deprecated; see the twin pair */
    }
    echo (new ReflectionFunction($name))->__toString(), "\n";
}

echo "-- the three handle classes are opaque\n";
foreach (['OpenSSLCertificate', 'OpenSSLCertificateSigningRequest', 'OpenSSLAsymmetricKey'] as $name) {
    $c = new ReflectionClass($name);
    printf("%-32s final=%d instantiable=%d methods=%d properties=%d constants=%d interfaces=%d\n",
        $name, (int) $c->isFinal(), (int) $c->isInstantiable(), count($c->getMethods()),
        count($c->getProperties()), count($c->getConstants()), count($c->getInterfaceNames()));
    try {
        new $name();
    } catch (Throwable $e) {
        echo '  new: ', get_class($e), ': ', $e->getMessage(), "\n";
    }
}
--EXPECT--
-- the extension
bool(true)
61 functions
openssl_x509_export_to_file
openssl_x509_export
openssl_x509_fingerprint
openssl_x509_check_private_key
openssl_x509_verify
openssl_x509_parse
openssl_x509_checkpurpose
openssl_x509_read
openssl_pkcs12_export_to_file
openssl_pkcs12_export
openssl_pkcs12_read
openssl_csr_export_to_file
openssl_csr_export
openssl_csr_sign
openssl_csr_new
openssl_csr_get_subject
openssl_csr_get_public_key
openssl_pkey_new
openssl_pkey_export_to_file
openssl_pkey_export
openssl_pkey_get_public
openssl_get_publickey
openssl_pkey_get_private
openssl_get_privatekey
openssl_pkey_get_details
openssl_pbkdf2
openssl_pkcs7_verify
openssl_pkcs7_encrypt
openssl_pkcs7_sign
openssl_pkcs7_decrypt
openssl_pkcs7_read
openssl_cms_verify
openssl_cms_encrypt
openssl_cms_sign
openssl_cms_decrypt
openssl_cms_read
openssl_private_encrypt
openssl_private_decrypt
openssl_public_encrypt
openssl_public_decrypt
openssl_error_string
openssl_sign
openssl_verify
openssl_seal
openssl_open
openssl_get_md_methods
openssl_get_cipher_methods
openssl_get_curve_names
openssl_digest
openssl_encrypt
openssl_decrypt
openssl_cipher_iv_length
openssl_cipher_key_length
openssl_dh_compute_key
openssl_pkey_derive
openssl_random_pseudo_bytes
openssl_spki_new
openssl_spki_verify
openssl_spki_export
openssl_spki_export_challenge
openssl_get_cert_locations
OpenSSLCertificate, OpenSSLCertificateSigningRequest, OpenSSLAsymmetricKey
OPENSSL_VERSION_TEXT             string
OPENSSL_VERSION_NUMBER           int
X509_PURPOSE_SSL_CLIENT          1
X509_PURPOSE_SSL_SERVER          2
X509_PURPOSE_NS_SSL_SERVER       3
X509_PURPOSE_SMIME_SIGN          4
X509_PURPOSE_SMIME_ENCRYPT       5
X509_PURPOSE_CRL_SIGN            6
X509_PURPOSE_ANY                 7
X509_PURPOSE_OCSP_HELPER         8
X509_PURPOSE_TIMESTAMP_SIGN      9
OPENSSL_ALGO_SHA1                1
OPENSSL_ALGO_MD5                 2
OPENSSL_ALGO_MD4                 3
OPENSSL_ALGO_SHA224              6
OPENSSL_ALGO_SHA256              7
OPENSSL_ALGO_SHA384              8
OPENSSL_ALGO_SHA512              9
OPENSSL_ALGO_RMD160              10
PKCS7_DETACHED                   64
PKCS7_TEXT                       1
PKCS7_NOINTERN                   16
PKCS7_NOVERIFY                   32
PKCS7_NOCHAIN                    8
PKCS7_NOCERTS                    2
PKCS7_NOATTR                     256
PKCS7_BINARY                     128
PKCS7_NOSIGS                     4
PKCS7_NOOLDMIMETYPE              1024
PKCS7_NOSMIMECAP                 512
PKCS7_CRLFEOL                    2048
PKCS7_NOCRL                      8192
PKCS7_NO_DUAL_CONTENT            65536
OPENSSL_CMS_DETACHED             64
OPENSSL_CMS_TEXT                 1
OPENSSL_CMS_NOINTERN             16
OPENSSL_CMS_NOVERIFY             32
OPENSSL_CMS_NOCERTS              2
OPENSSL_CMS_NOATTR               256
OPENSSL_CMS_BINARY               128
OPENSSL_CMS_NOSIGS               12
OPENSSL_CMS_OLDMIMETYPE          1024
OPENSSL_PKCS1_PADDING            1
OPENSSL_NO_PADDING               3
OPENSSL_PKCS1_OAEP_PADDING       4
OPENSSL_PKCS1_PSS_PADDING        6
OPENSSL_DEFAULT_STREAM_CIPHERS   'ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:DHE-DSS-AES128-GCM-SHA256:kEDH+AESGCM:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA:ECDHE-ECDSA-AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-AES128-SHA:DHE-DSS-AES128-SHA256:DHE-RSA-AES256-SHA256:DHE-DSS-AES256-SHA:DHE-RSA-AES256-SHA:AES128-GCM-SHA256:AES256-GCM-SHA384:AES128:AES256:HIGH:!SSLv2:!aNULL:!eNULL:!EXPORT:!DES:!MD5:!RC4:!ADH'
OPENSSL_CIPHER_RC2_40            0
OPENSSL_CIPHER_RC2_128           1
OPENSSL_CIPHER_RC2_64            2
OPENSSL_CIPHER_DES               3
OPENSSL_CIPHER_3DES              4
OPENSSL_CIPHER_AES_128_CBC       5
OPENSSL_CIPHER_AES_192_CBC       6
OPENSSL_CIPHER_AES_256_CBC       7
OPENSSL_KEYTYPE_RSA              0
OPENSSL_KEYTYPE_DSA              1
OPENSSL_KEYTYPE_DH               2
OPENSSL_KEYTYPE_EC               3
OPENSSL_KEYTYPE_X25519           4
OPENSSL_KEYTYPE_ED25519          5
OPENSSL_KEYTYPE_X448             6
OPENSSL_KEYTYPE_ED448            7
OPENSSL_RAW_DATA                 1
OPENSSL_ZERO_PADDING             2
OPENSSL_DONT_ZERO_PAD_KEY        4
OPENSSL_TLSEXT_SERVER_NAME       1
OPENSSL_ENCODING_DER             0
OPENSSL_ENCODING_SMIME           1
OPENSSL_ENCODING_PEM             2
openssl.cafile                   NULL
openssl.capath                   NULL
openssl.cafile                   access=2 global=NULL
openssl.capath                   access=2 global=NULL
-- every function's signature
Function [ <internal:openssl> function openssl_x509_export_to_file ] {

  - Parameters [3] {
    Parameter #0 [ <required> OpenSSLCertificate|string $certificate ]
    Parameter #1 [ <required> string $output_filename ]
    Parameter #2 [ <optional> bool $no_text = true ]
  }
  - Return [ bool ]
}

Function [ <internal:openssl> function openssl_x509_export ] {

  - Parameters [3] {
    Parameter #0 [ <required> OpenSSLCertificate|string $certificate ]
    Parameter #1 [ <required> &$output ]
    Parameter #2 [ <optional> bool $no_text = true ]
  }
  - Return [ bool ]
}

Function [ <internal:openssl> function openssl_x509_fingerprint ] {

  - Parameters [3] {
    Parameter #0 [ <required> OpenSSLCertificate|string $certificate ]
    Parameter #1 [ <optional> string $digest_algo = "sha1" ]
    Parameter #2 [ <optional> bool $binary = false ]
  }
  - Return [ string|false ]
}

Function [ <internal:openssl> function openssl_x509_check_private_key ] {

  - Parameters [2] {
    Parameter #0 [ <required> OpenSSLCertificate|string $certificate ]
    Parameter #1 [ <required> $private_key ]
  }
  - Return [ bool ]
}

Function [ <internal:openssl> function openssl_x509_verify ] {

  - Parameters [2] {
    Parameter #0 [ <required> OpenSSLCertificate|string $certificate ]
    Parameter #1 [ <required> $public_key ]
  }
  - Return [ int ]
}

Function [ <internal:openssl> function openssl_x509_parse ] {

  - Parameters [2] {
    Parameter #0 [ <required> OpenSSLCertificate|string $certificate ]
    Parameter #1 [ <optional> bool $short_names = true ]
  }
  - Return [ array|false ]
}

Function [ <internal:openssl> function openssl_x509_checkpurpose ] {

  - Parameters [4] {
    Parameter #0 [ <required> OpenSSLCertificate|string $certificate ]
    Parameter #1 [ <required> int $purpose ]
    Parameter #2 [ <optional> array $ca_info = [] ]
    Parameter #3 [ <optional> ?string $untrusted_certificates_file = null ]
  }
  - Return [ int|bool ]
}

Function [ <internal:openssl> function openssl_x509_read ] {

  - Parameters [1] {
    Parameter #0 [ <required> OpenSSLCertificate|string $certificate ]
  }
  - Return [ OpenSSLCertificate|false ]
}

Function [ <internal:openssl> function openssl_pkcs12_export_to_file ] {

  - Parameters [5] {
    Parameter #0 [ <required> OpenSSLCertificate|string $certificate ]
    Parameter #1 [ <required> string $output_filename ]
    Parameter #2 [ <required> $private_key ]
    Parameter #3 [ <required> string $passphrase ]
    Parameter #4 [ <optional> array $options = [] ]
  }
  - Return [ bool ]
}

Function [ <internal:openssl> function openssl_pkcs12_export ] {

  - Parameters [5] {
    Parameter #0 [ <required> OpenSSLCertificate|string $certificate ]
    Parameter #1 [ <required> &$output ]
    Parameter #2 [ <required> $private_key ]
    Parameter #3 [ <required> string $passphrase ]
    Parameter #4 [ <optional> array $options = [] ]
  }
  - Return [ bool ]
}

Function [ <internal:openssl> function openssl_pkcs12_read ] {

  - Parameters [3] {
    Parameter #0 [ <required> string $pkcs12 ]
    Parameter #1 [ <required> &$certificates ]
    Parameter #2 [ <required> string $passphrase ]
  }
  - Return [ bool ]
}

Function [ <internal:openssl> function openssl_csr_export_to_file ] {

  - Parameters [3] {
    Parameter #0 [ <required> OpenSSLCertificateSigningRequest|string $csr ]
    Parameter #1 [ <required> string $output_filename ]
    Parameter #2 [ <optional> bool $no_text = true ]
  }
  - Return [ bool ]
}

Function [ <internal:openssl> function openssl_csr_export ] {

  - Parameters [3] {
    Parameter #0 [ <required> OpenSSLCertificateSigningRequest|string $csr ]
    Parameter #1 [ <required> &$output ]
    Parameter #2 [ <optional> bool $no_text = true ]
  }
  - Return [ bool ]
}

Function [ <internal:openssl> function openssl_csr_sign ] {

  - Parameters [7] {
    Parameter #0 [ <required> OpenSSLCertificateSigningRequest|string $csr ]
    Parameter #1 [ <required> OpenSSLCertificate|string|null $ca_certificate ]
    Parameter #2 [ <required> $private_key ]
    Parameter #3 [ <required> int $days ]
    Parameter #4 [ <optional> ?array $options = null ]
    Parameter #5 [ <optional> int $serial = 0 ]
    Parameter #6 [ <optional> ?string $serial_hex = null ]
  }
  - Return [ OpenSSLCertificate|false ]
}

Function [ <internal:openssl> function openssl_csr_new ] {

  - Parameters [4] {
    Parameter #0 [ <required> array $distinguished_names ]
    Parameter #1 [ <required> &$private_key ]
    Parameter #2 [ <optional> ?array $options = null ]
    Parameter #3 [ <optional> ?array $extra_attributes = null ]
  }
  - Return [ OpenSSLCertificateSigningRequest|bool ]
}

Function [ <internal:openssl> function openssl_csr_get_subject ] {

  - Parameters [2] {
    Parameter #0 [ <required> OpenSSLCertificateSigningRequest|string $csr ]
    Parameter #1 [ <optional> bool $short_names = true ]
  }
  - Return [ array|false ]
}

Function [ <internal:openssl> function openssl_csr_get_public_key ] {

  - Parameters [2] {
    Parameter #0 [ <required> OpenSSLCertificateSigningRequest|string $csr ]
    Parameter #1 [ <optional> bool $short_names = true ]
  }
  - Return [ OpenSSLAsymmetricKey|false ]
}

Function [ <internal:openssl> function openssl_pkey_new ] {

  - Parameters [1] {
    Parameter #0 [ <optional> ?array $options = null ]
  }
  - Return [ OpenSSLAsymmetricKey|false ]
}

Function [ <internal:openssl> function openssl_pkey_export_to_file ] {

  - Parameters [4] {
    Parameter #0 [ <required> $key ]
    Parameter #1 [ <required> string $output_filename ]
    Parameter #2 [ <optional> ?string $passphrase = null ]
    Parameter #3 [ <optional> ?array $options = null ]
  }
  - Return [ bool ]
}

Function [ <internal:openssl> function openssl_pkey_export ] {

  - Parameters [4] {
    Parameter #0 [ <required> $key ]
    Parameter #1 [ <required> &$output ]
    Parameter #2 [ <optional> ?string $passphrase = null ]
    Parameter #3 [ <optional> ?array $options = null ]
  }
  - Return [ bool ]
}

Function [ <internal:openssl> function openssl_pkey_get_public ] {

  - Parameters [1] {
    Parameter #0 [ <required> $public_key ]
  }
  - Return [ OpenSSLAsymmetricKey|false ]
}

Function [ <internal:openssl> function openssl_get_publickey ] {

  - Parameters [1] {
    Parameter #0 [ <required> $public_key ]
  }
  - Return [ OpenSSLAsymmetricKey|false ]
}

Function [ <internal:openssl> function openssl_pkey_get_private ] {

  - Parameters [2] {
    Parameter #0 [ <required> $private_key ]
    Parameter #1 [ <optional> ?string $passphrase = null ]
  }
  - Return [ OpenSSLAsymmetricKey|false ]
}

Function [ <internal:openssl> function openssl_get_privatekey ] {

  - Parameters [2] {
    Parameter #0 [ <required> $private_key ]
    Parameter #1 [ <optional> ?string $passphrase = null ]
  }
  - Return [ OpenSSLAsymmetricKey|false ]
}

Function [ <internal:openssl> function openssl_pkey_get_details ] {

  - Parameters [1] {
    Parameter #0 [ <required> OpenSSLAsymmetricKey $key ]
  }
  - Return [ array|false ]
}

Function [ <internal:openssl> function openssl_pbkdf2 ] {

  - Parameters [5] {
    Parameter #0 [ <required> string $password ]
    Parameter #1 [ <required> string $salt ]
    Parameter #2 [ <required> int $key_length ]
    Parameter #3 [ <required> int $iterations ]
    Parameter #4 [ <optional> string $digest_algo = "sha1" ]
  }
  - Return [ string|false ]
}

Function [ <internal:openssl> function openssl_pkcs7_verify ] {

  - Parameters [7] {
    Parameter #0 [ <required> string $input_filename ]
    Parameter #1 [ <required> int $flags ]
    Parameter #2 [ <optional> ?string $signers_certificates_filename = null ]
    Parameter #3 [ <optional> array $ca_info = [] ]
    Parameter #4 [ <optional> ?string $untrusted_certificates_filename = null ]
    Parameter #5 [ <optional> ?string $content = null ]
    Parameter #6 [ <optional> ?string $output_filename = null ]
  }
  - Return [ int|bool ]
}

Function [ <internal:openssl> function openssl_pkcs7_encrypt ] {

  - Parameters [6] {
    Parameter #0 [ <required> string $input_filename ]
    Parameter #1 [ <required> string $output_filename ]
    Parameter #2 [ <required> $certificate ]
    Parameter #3 [ <required> ?array $headers ]
    Parameter #4 [ <optional> int $flags = 0 ]
    Parameter #5 [ <optional> int $cipher_algo = OPENSSL_CIPHER_AES_128_CBC ]
  }
  - Return [ bool ]
}

Function [ <internal:openssl> function openssl_pkcs7_sign ] {

  - Parameters [7] {
    Parameter #0 [ <required> string $input_filename ]
    Parameter #1 [ <required> string $output_filename ]
    Parameter #2 [ <required> OpenSSLCertificate|string $certificate ]
    Parameter #3 [ <required> $private_key ]
    Parameter #4 [ <required> ?array $headers ]
    Parameter #5 [ <optional> int $flags = PKCS7_DETACHED ]
    Parameter #6 [ <optional> ?string $untrusted_certificates_filename = null ]
  }
  - Return [ bool ]
}

Function [ <internal:openssl> function openssl_pkcs7_decrypt ] {

  - Parameters [4] {
    Parameter #0 [ <required> string $input_filename ]
    Parameter #1 [ <required> string $output_filename ]
    Parameter #2 [ <required> $certificate ]
    Parameter #3 [ <optional> $private_key = null ]
  }
  - Return [ bool ]
}

Function [ <internal:openssl> function openssl_pkcs7_read ] {

  - Parameters [2] {
    Parameter #0 [ <required> string $data ]
    Parameter #1 [ <required> &$certificates ]
  }
  - Return [ bool ]
}

Function [ <internal:openssl> function openssl_cms_verify ] {

  - Parameters [9] {
    Parameter #0 [ <required> string $input_filename ]
    Parameter #1 [ <optional> int $flags = 0 ]
    Parameter #2 [ <optional> ?string $certificates = null ]
    Parameter #3 [ <optional> array $ca_info = [] ]
    Parameter #4 [ <optional> ?string $untrusted_certificates_filename = null ]
    Parameter #5 [ <optional> ?string $content = null ]
    Parameter #6 [ <optional> ?string $pk7 = null ]
    Parameter #7 [ <optional> ?string $sigfile = null ]
    Parameter #8 [ <optional> int $encoding = OPENSSL_ENCODING_SMIME ]
  }
  - Return [ bool ]
}

Function [ <internal:openssl> function openssl_cms_encrypt ] {

  - Parameters [7] {
    Parameter #0 [ <required> string $input_filename ]
    Parameter #1 [ <required> string $output_filename ]
    Parameter #2 [ <required> $certificate ]
    Parameter #3 [ <required> ?array $headers ]
    Parameter #4 [ <optional> int $flags = 0 ]
    Parameter #5 [ <optional> int $encoding = OPENSSL_ENCODING_SMIME ]
    Parameter #6 [ <optional> string|int $cipher_algo = OPENSSL_CIPHER_AES_128_CBC ]
  }
  - Return [ bool ]
}

Function [ <internal:openssl> function openssl_cms_sign ] {

  - Parameters [8] {
    Parameter #0 [ <required> string $input_filename ]
    Parameter #1 [ <required> string $output_filename ]
    Parameter #2 [ <required> OpenSSLCertificate|string $certificate ]
    Parameter #3 [ <required> $private_key ]
    Parameter #4 [ <required> ?array $headers ]
    Parameter #5 [ <optional> int $flags = 0 ]
    Parameter #6 [ <optional> int $encoding = OPENSSL_ENCODING_SMIME ]
    Parameter #7 [ <optional> ?string $untrusted_certificates_filename = null ]
  }
  - Return [ bool ]
}

Function [ <internal:openssl> function openssl_cms_decrypt ] {

  - Parameters [5] {
    Parameter #0 [ <required> string $input_filename ]
    Parameter #1 [ <required> string $output_filename ]
    Parameter #2 [ <required> $certificate ]
    Parameter #3 [ <optional> $private_key = null ]
    Parameter #4 [ <optional> int $encoding = OPENSSL_ENCODING_SMIME ]
  }
  - Return [ bool ]
}

Function [ <internal:openssl> function openssl_cms_read ] {

  - Parameters [2] {
    Parameter #0 [ <required> string $input_filename ]
    Parameter #1 [ <required> &$certificates ]
  }
  - Return [ bool ]
}

Function [ <internal:openssl> function openssl_private_encrypt ] {

  - Parameters [4] {
    Parameter #0 [ <required> string $data ]
    Parameter #1 [ <required> &$encrypted_data ]
    Parameter #2 [ <required> $private_key ]
    Parameter #3 [ <optional> int $padding = OPENSSL_PKCS1_PADDING ]
  }
  - Return [ bool ]
}

Function [ <internal:openssl> function openssl_private_decrypt ] {

  - Parameters [5] {
    Parameter #0 [ <required> string $data ]
    Parameter #1 [ <required> &$decrypted_data ]
    Parameter #2 [ <required> $private_key ]
    Parameter #3 [ <optional> int $padding = OPENSSL_PKCS1_PADDING ]
    Parameter #4 [ <optional> ?string $digest_algo = null ]
  }
  - Return [ bool ]
}

Function [ <internal:openssl> function openssl_public_encrypt ] {

  - Parameters [5] {
    Parameter #0 [ <required> string $data ]
    Parameter #1 [ <required> &$encrypted_data ]
    Parameter #2 [ <required> $public_key ]
    Parameter #3 [ <optional> int $padding = OPENSSL_PKCS1_PADDING ]
    Parameter #4 [ <optional> ?string $digest_algo = null ]
  }
  - Return [ bool ]
}

Function [ <internal:openssl> function openssl_public_decrypt ] {

  - Parameters [4] {
    Parameter #0 [ <required> string $data ]
    Parameter #1 [ <required> &$decrypted_data ]
    Parameter #2 [ <required> $public_key ]
    Parameter #3 [ <optional> int $padding = OPENSSL_PKCS1_PADDING ]
  }
  - Return [ bool ]
}

Function [ <internal:openssl> function openssl_error_string ] {

  - Parameters [0] {
  }
  - Return [ string|false ]
}

Function [ <internal:openssl> function openssl_sign ] {

  - Parameters [5] {
    Parameter #0 [ <required> string $data ]
    Parameter #1 [ <required> &$signature ]
    Parameter #2 [ <required> $private_key ]
    Parameter #3 [ <optional> string|int $algorithm = OPENSSL_ALGO_SHA1 ]
    Parameter #4 [ <optional> int $padding = 0 ]
  }
  - Return [ bool ]
}

Function [ <internal:openssl> function openssl_verify ] {

  - Parameters [5] {
    Parameter #0 [ <required> string $data ]
    Parameter #1 [ <required> string $signature ]
    Parameter #2 [ <required> $public_key ]
    Parameter #3 [ <optional> string|int $algorithm = OPENSSL_ALGO_SHA1 ]
    Parameter #4 [ <optional> int $padding = 0 ]
  }
  - Return [ int|false ]
}

Function [ <internal:openssl> function openssl_seal ] {

  - Parameters [6] {
    Parameter #0 [ <required> string $data ]
    Parameter #1 [ <required> &$sealed_data ]
    Parameter #2 [ <required> &$encrypted_keys ]
    Parameter #3 [ <required> array $public_key ]
    Parameter #4 [ <required> string $cipher_algo ]
    Parameter #5 [ <optional> &$iv = null ]
  }
  - Return [ int|false ]
}

Function [ <internal:openssl> function openssl_open ] {

  - Parameters [6] {
    Parameter #0 [ <required> string $data ]
    Parameter #1 [ <required> &$output ]
    Parameter #2 [ <required> string $encrypted_key ]
    Parameter #3 [ <required> $private_key ]
    Parameter #4 [ <required> string $cipher_algo ]
    Parameter #5 [ <optional> ?string $iv = null ]
  }
  - Return [ bool ]
}

Function [ <internal:openssl> function openssl_get_md_methods ] {

  - Parameters [1] {
    Parameter #0 [ <optional> bool $aliases = false ]
  }
  - Return [ array ]
}

Function [ <internal:openssl> function openssl_get_cipher_methods ] {

  - Parameters [1] {
    Parameter #0 [ <optional> bool $aliases = false ]
  }
  - Return [ array ]
}

Function [ <internal:openssl> function openssl_get_curve_names ] {

  - Parameters [0] {
  }
  - Return [ array|false ]
}

Function [ <internal:openssl> function openssl_digest ] {

  - Parameters [3] {
    Parameter #0 [ <required> string $data ]
    Parameter #1 [ <required> string $digest_algo ]
    Parameter #2 [ <optional> bool $binary = false ]
  }
  - Return [ string|false ]
}

Function [ <internal:openssl> function openssl_encrypt ] {

  - Parameters [8] {
    Parameter #0 [ <required> string $data ]
    Parameter #1 [ <required> string $cipher_algo ]
    Parameter #2 [ <required> string $passphrase ]
    Parameter #3 [ <optional> int $options = 0 ]
    Parameter #4 [ <optional> string $iv = "" ]
    Parameter #5 [ <optional> &$tag = null ]
    Parameter #6 [ <optional> string $aad = "" ]
    Parameter #7 [ <optional> int $tag_length = 16 ]
  }
  - Return [ string|false ]
}

Function [ <internal:openssl> function openssl_decrypt ] {

  - Parameters [7] {
    Parameter #0 [ <required> string $data ]
    Parameter #1 [ <required> string $cipher_algo ]
    Parameter #2 [ <required> string $passphrase ]
    Parameter #3 [ <optional> int $options = 0 ]
    Parameter #4 [ <optional> string $iv = "" ]
    Parameter #5 [ <optional> ?string $tag = null ]
    Parameter #6 [ <optional> string $aad = "" ]
  }
  - Return [ string|false ]
}

Function [ <internal:openssl> function openssl_cipher_iv_length ] {

  - Parameters [1] {
    Parameter #0 [ <required> string $cipher_algo ]
  }
  - Return [ int|false ]
}

Function [ <internal:openssl> function openssl_cipher_key_length ] {

  - Parameters [1] {
    Parameter #0 [ <required> string $cipher_algo ]
  }
  - Return [ int|false ]
}

Function [ <internal:openssl> function openssl_dh_compute_key ] {

  - Parameters [2] {
    Parameter #0 [ <required> string $public_key ]
    Parameter #1 [ <required> OpenSSLAsymmetricKey $private_key ]
  }
  - Return [ string|false ]
}

Function [ <internal:openssl> function openssl_random_pseudo_bytes ] {

  - Parameters [2] {
    Parameter #0 [ <required> int $length ]
    Parameter #1 [ <optional> &$strong_result = null ]
  }
  - Return [ string ]
}

Function [ <internal:openssl> function openssl_spki_new ] {

  - Parameters [3] {
    Parameter #0 [ <required> OpenSSLAsymmetricKey $private_key ]
    Parameter #1 [ <required> string $challenge ]
    Parameter #2 [ <optional> int $digest_algo = OPENSSL_ALGO_MD5 ]
  }
  - Return [ string|false ]
}

Function [ <internal:openssl> function openssl_spki_verify ] {

  - Parameters [1] {
    Parameter #0 [ <required> string $spki ]
  }
  - Return [ bool ]
}

Function [ <internal:openssl> function openssl_spki_export ] {

  - Parameters [1] {
    Parameter #0 [ <required> string $spki ]
  }
  - Return [ string|false ]
}

Function [ <internal:openssl> function openssl_spki_export_challenge ] {

  - Parameters [1] {
    Parameter #0 [ <required> string $spki ]
  }
  - Return [ string|false ]
}

Function [ <internal:openssl> function openssl_get_cert_locations ] {

  - Parameters [0] {
  }
  - Return [ array ]
}

-- the three handle classes are opaque
OpenSSLCertificate               final=1 instantiable=1 methods=0 properties=0 constants=0 interfaces=0
  new: Error: Cannot directly construct OpenSSLCertificate, use openssl_x509_read() instead
OpenSSLCertificateSigningRequest final=1 instantiable=1 methods=0 properties=0 constants=0 interfaces=0
  new: Error: Cannot directly construct OpenSSLCertificateSigningRequest, use openssl_csr_new() instead
OpenSSLAsymmetricKey             final=1 instantiable=1 methods=0 properties=0 constants=0 interfaces=0
  new: Error: Cannot directly construct OpenSSLAsymmetricKey, use openssl_pkey_new() instead
