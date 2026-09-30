--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
POLICY DIVERGENCE §10: ext/openssl's deprecated names and parameter are refused (PHL half)
--DESCRIPTION--
php marks three of ext/openssl's functions `deprecated since 8.0` --
openssl_x509_free(), openssl_pkey_free() and openssl_free_key(), all three
no-ops since a certificate and a key became objects -- and deprecates
openssl_pkey_derive()'s `$key_length` parameter, warning that it "is either
ignored or truncates the key". PHL targets php's NON-deprecated surface (§10),
so the three names are not registered at all and the third parameter is not
declared. php's half is the `_zend` twin.

The passphrase PROMPT is here too, and it is not a §10 item but a refusal to
block: OpenSSL's default UI reads /dev/tty when a private key needs a
passphrase it was not given, so php stops a non-interactive program dead
waiting for a human. PHL installs a callback that answers what it has -- an
empty string when it has nothing -- so the read simply fails. It is untestable
in the php half for the same reason it exists.
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL half of the twin pair; php's behaviour is in the _zend twin";
}
?>
--FILE--
<?php
foreach (['openssl_x509_free', 'openssl_pkey_free', 'openssl_free_key'] as $name) {
    var_dump(function_exists($name));
}
var_dump(in_array('openssl_pkey_free', get_extension_funcs('openssl'), true));
var_dump(in_array('openssl_free_key', (new ReflectionExtension('openssl'))->getFunctions()
    ? array_keys((new ReflectionExtension('openssl'))->getFunctions()) : [], true));

echo "-- the aliases php does NOT deprecate are still here\n";
var_dump(function_exists('openssl_get_publickey'), function_exists('openssl_get_privatekey'));

echo "-- openssl_pkey_derive() declares two parameters\n";
$r = new ReflectionFunction('openssl_pkey_derive');
var_dump($r->getNumberOfParameters(), $r->getNumberOfRequiredParameters());
$a = openssl_pkey_new(['private_key_type' => OPENSSL_KEYTYPE_EC, 'curve_name' => 'prime256v1']);
$b = openssl_pkey_new(['private_key_type' => OPENSSL_KEYTYPE_EC, 'curve_name' => 'prime256v1']);
$aPub = openssl_pkey_get_details($a)['key'];
var_dump(strlen((string) openssl_pkey_derive($aPub, $b)));
try { openssl_pkey_derive($aPub, $b, 0); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { openssl_pkey_derive($aPub, $b, 16); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

echo "-- an encrypted key never prompts: it answers false\n";
$k = openssl_pkey_new(['private_key_bits' => 1024]);
$enc = null;
openssl_pkey_export($k, $enc, 'the-passphrase');
var_dump(openssl_pkey_get_private($enc));
var_dump(openssl_pkey_get_private($enc, 'wrong'));
var_dump(openssl_pkey_get_private([$enc, 'wrong']));
var_dump(get_debug_type(openssl_pkey_get_private($enc, 'the-passphrase')));
while (openssl_error_string() !== false) { /* drain */ }
?>
--EXPECT--
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
-- the aliases php does NOT deprecate are still here
bool(true)
bool(true)
-- openssl_pkey_derive() declares two parameters
int(2)
int(2)
int(32)
ArgumentCountError: openssl_pkey_derive() expects exactly 2 arguments, 3 given
ArgumentCountError: openssl_pkey_derive() expects exactly 2 arguments, 3 given
-- an encrypted key never prompts: it answers false
bool(false)
bool(false)
bool(false)
string(20) "OpenSSLAsymmetricKey"
