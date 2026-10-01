--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
POLICY DIVERGENCE the scope policy: ext/openssl's deprecated names and parameter exist and warn (php half)
--DESCRIPTION--
The php half of openssl_policy_divergence.phpt: php still registers
openssl_x509_free(), openssl_pkey_free() and openssl_free_key() as no-ops that
raise E_DEPRECATED, and still takes openssl_pkey_derive()'s third parameter
with a deprecation of its own. PHL has no engine deprecation sites and removes
the spellings instead (the scope policy). The passphrase PROMPT the PHL half covers is not
exercised here: php reads /dev/tty for it, which a corpus cannot drive.
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip php half of the twin pair; PHL's behaviour is in the non-_zend member";
}
?>
--FILE--
<?php
set_error_handler(static function ($n, $m) { echo 'D: ', $m, "\n"; return true; });
foreach (['openssl_x509_free', 'openssl_pkey_free', 'openssl_free_key'] as $name) {
    var_dump(function_exists($name));
}
var_dump(in_array('openssl_pkey_free', get_extension_funcs('openssl'), true));

echo "-- the aliases php does NOT deprecate are still here\n";
var_dump(function_exists('openssl_get_publickey'), function_exists('openssl_get_privatekey'));

echo "-- openssl_pkey_derive() declares three parameters\n";
$r = new ReflectionFunction('openssl_pkey_derive');
var_dump($r->getNumberOfParameters(), $r->getNumberOfRequiredParameters());
$a = openssl_pkey_new(['private_key_type' => OPENSSL_KEYTYPE_EC, 'curve_name' => 'prime256v1']);
$b = openssl_pkey_new(['private_key_type' => OPENSSL_KEYTYPE_EC, 'curve_name' => 'prime256v1']);
$aPub = openssl_pkey_get_details($a)['key'];
var_dump(strlen((string) openssl_pkey_derive($aPub, $b)));
var_dump(strlen((string) openssl_pkey_derive($aPub, $b, 0)));
var_dump(strlen((string) openssl_pkey_derive($aPub, $b, 16)));

echo "-- and the three no-ops answer null after deprecating\n";
$k = openssl_pkey_new(['private_key_bits' => 1024]);
var_dump(openssl_pkey_free($k));
var_dump(openssl_free_key($k));
while (openssl_error_string() !== false) { /* drain */ }
restore_error_handler();
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
-- the aliases php does NOT deprecate are still here
bool(true)
bool(true)
-- openssl_pkey_derive() declares three parameters
int(3)
int(2)
int(32)
D: openssl_pkey_derive(): the $key_length parameter is deprecated as it is either ignored or truncates the key
int(32)
D: openssl_pkey_derive(): the $key_length parameter is deprecated as it is either ignored or truncates the key
int(16)
-- and the three no-ops answer null after deprecating
D: Function openssl_pkey_free() is deprecated since 8.0, as OpenSSLAsymmetricKey objects are freed automatically
NULL
D: Function openssl_free_key() is deprecated since 8.0, as OpenSSLAsymmetricKey objects are freed automatically
NULL
