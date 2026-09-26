--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/curl is loaded, and its library-wide surface (curl_version + the three strerrors) answers
--SKIPIF--
<?php
if (!extension_loaded('curl')) {
    echo "skip ext/curl not available";
}
?>
--FILE--
<?php
// Everything here has to hold against ANY libcurl >= 8.5, so the library's own
// numbers and version strings are asserted by SHAPE, never by value; the two
// message strings pinned below are libcurl's oldest and most stable.

var_dump(extension_loaded('curl'), extension_loaded('CURL'));
var_dump(in_array('curl', get_loaded_extensions(), true));

$v = curl_version();
echo implode(',', array_keys($v)), "\n";
foreach ($v as $k => $x) {
    // The protocol LIST is the linked library's, and a Schannel or a
    // feature-trimmed build carries a different one -- so only its type is
    // asserted here. feature_list is php's own static table and does not move.
    echo $k, '=', gettype($x), "\n";
}
echo implode(',', array_keys($v['feature_list'])), "\n";
var_dump(count(array_filter($v['feature_list'], 'is_bool')) === count($v['feature_list']));
var_dump($v['version'] !== '' && $v['host'] !== '');
var_dump(in_array('http', $v['protocols'], true), in_array('https', $v['protocols'], true));

// CURLE_OK / CURLM_OK / CURLSHE_OK. Every other code's wording belongs to the
// linked library and moves with it.
var_dump(curl_strerror(0), curl_multi_strerror(0), curl_share_strerror(0));
var_dump(is_string(curl_strerror(3)), curl_strerror(3) !== '');

// The code is narrowed to the C enum's width before the library sees it, so a
// value no enum could hold answers as its truncation -- PHP_INT_MAX arrives as
// -1 (CURLM_CALL_MULTI_PERFORM), which is why this pair is identical.
var_dump(curl_multi_strerror(PHP_INT_MAX) === curl_multi_strerror(-1));
var_dump(curl_strerror(PHP_INT_MIN) === curl_strerror(0));

foreach ([
    'curl_strerror()' => fn() => curl_strerror(),
    'curl_strerror(1, 2)' => fn() => curl_strerror(1, 2),
    'curl_strerror("x")' => fn() => curl_strerror("x"),
    'curl_version(1)' => fn() => curl_version(1),
] as $label => $call) {
    try {
        $call();
        echo $label, " => no throw\n";
    } catch (Throwable $e) {
        echo $label, ' => ', get_class($e), ': ', $e->getMessage(), "\n";
    }
}

foreach (['curl_version', 'curl_strerror', 'curl_multi_strerror', 'curl_share_strerror'] as $f) {
    $r = new ReflectionFunction($f);
    $ps = [];
    foreach ($r->getParameters() as $p) {
        $ps[] = (string)$p->getType() . ' $' . $p->getName();
    }
    echo $f, '(', implode(', ', $ps), '): ', (string)$r->getReturnType(),
        ' internal=', var_export($r->isInternal(), true), "\n";
}
--EXPECT--
bool(true)
bool(true)
bool(true)
version_number,age,features,feature_list,ssl_version_number,version,host,ssl_version,libz_version,protocols,ares,ares_num,libidn,iconv_ver_num,libssh_version,brotli_ver_num,brotli_version
version_number=integer
age=integer
features=integer
feature_list=array
ssl_version_number=integer
version=string
host=string
ssl_version=string
libz_version=string
protocols=array
ares=string
ares_num=integer
libidn=string
iconv_ver_num=integer
libssh_version=string
brotli_ver_num=integer
brotli_version=string
AsynchDNS,CharConv,Debug,GSS-Negotiate,IDN,IPv6,krb4,Largefile,libz,NTLM,NTLMWB,SPNEGO,SSL,SSPI,TLS-SRP,HTTP2,GSSAPI,KERBEROS5,UNIX_SOCKETS,PSL,HTTPS_PROXY,MULTI_SSL,BROTLI,ALTSVC,HTTP3,UNICODE,ZSTD,HSTS,GSASL
bool(true)
bool(true)
bool(true)
bool(true)
string(8) "No error"
string(8) "No error"
string(8) "No error"
bool(true)
bool(true)
bool(true)
bool(true)
curl_strerror() => ArgumentCountError: curl_strerror() expects exactly 1 argument, 0 given
curl_strerror(1, 2) => ArgumentCountError: curl_strerror() expects exactly 1 argument, 2 given
curl_strerror("x") => TypeError: curl_strerror(): Argument #1 ($error_code) must be of type int, string given
curl_version(1) => ArgumentCountError: curl_version() expects exactly 0 arguments, 1 given
curl_version(): array|false internal=true
curl_strerror(int $error_code): ?string internal=true
curl_multi_strerror(int $error_code): ?string internal=true
curl_share_strerror(int $error_code): ?string internal=true
