--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
curl_getinfo() answers php's 41-key array, or one selector, and never throws
--DESCRIPTION--
A CURLINFO_* carries its own type in the NUMBER -- libcurl masks the top bits
with CURLINFO_TYPEMASK -- and all 75 selectors php exposes agree with their
bucket, so the selector form needs no table, only the mask. The eight names
that are not infos at all (CURLINFO_TEXT and the rest of the DEBUGFUNCTION set,
plus CURLINFO_LASTONE) fall outside every bucket, which is why php answers
false for them, the same false an unknown number gets. No selector throws.

The no-selector ARRAY is a different thing with its own order, and the same
missing value is rendered three ways depending on how it was asked for: through
a selector a string info libcurl left NULL is false; in the array it is the
EMPTY STRING -- except content_type, which stays NULL. That is one row's worth
of behaviour no reading of the manual would produce.

Nothing here pins capath/cainfo's defaults or any timing: those are the build's
and the run's, not php's.
--SKIPIF--
<?php
if (!extension_loaded('curl')) {
    echo "skip ext/curl not available";
}
?>
--FILE--
<?php
// Five keys exist only past a libcurl version -- queue_time_us (8.6), used_proxy
// (8.7), posttransfer_time_us (8.10), httpauth_used and proxyauth_used (8.12) --
// so the library a build links decides whether they are there. The dump counts
// and prints the 41 every build has; the rest are checked by name and place:
// every key present is one of php's 46, in php's order.
$gated = ['queue_time_us', 'posttransfer_time_us', 'used_proxy', 'httpauth_used', 'proxyauth_used'];
$order = ['url', 'content_type', 'http_code', 'header_size', 'request_size',
    'filetime', 'ssl_verify_result', 'redirect_count', 'total_time',
    'namelookup_time', 'connect_time', 'pretransfer_time', 'size_upload',
    'size_download', 'speed_download', 'speed_upload',
    'download_content_length', 'upload_content_length', 'starttransfer_time',
    'redirect_time', 'redirect_url', 'primary_ip', 'certinfo', 'primary_port',
    'local_ip', 'local_port', 'http_version', 'protocol', 'ssl_verifyresult',
    'scheme', 'appconnect_time_us', 'queue_time_us', 'connect_time_us',
    'namelookup_time_us', 'pretransfer_time_us', 'redirect_time_us',
    'starttransfer_time_us', 'posttransfer_time_us', 'total_time_us',
    'effective_method', 'capath', 'cainfo', 'used_proxy', 'httpauth_used',
    'proxyauth_used', 'conn_id'];
foreach ([null, 'http://example.com/path', 'https://x.example:8443/a?b=c'] as $u) {
    $h = $u === null ? curl_init() : curl_init($u);
    $a = curl_getinfo($h);
    $keys = array_keys($a);
    echo '== ', var_export($u, true), ' keys=', count(array_diff($keys, $gated)),
        ' order=', var_export(array_values(array_intersect($order, $keys)) === $keys, true), "\n";
    foreach ($a as $k => $v) {
        if (in_array($k, $gated, true)) {
            continue;
        }
        // capath and cainfo answer the BUILD's compiled-in defaults -- CAINFO
        // does not even reflect CURLOPT_CAINFO -- so only their type is stable.
        if ($k === 'capath' || $k === 'cainfo') { echo "  $k: ", gettype($v), "\n"; continue; }
        echo "  $k: ", gettype($v), ' ', is_array($v) ? 'array(' . count($v) . ')' : var_export($v, true), "\n";
    }
}

// the three renderings of a missing string
$h = curl_init('http://example.com/');
var_dump(curl_getinfo($h, CURLINFO_CONTENT_TYPE), curl_getinfo($h)['content_type']);
var_dump(curl_getinfo($h, CURLINFO_SCHEME), curl_getinfo($h)['scheme']);

// the buckets, one selector each
var_dump(curl_getinfo($h, CURLINFO_EFFECTIVE_URL));      // string
var_dump(curl_getinfo($h, CURLINFO_RESPONSE_CODE));      // long
var_dump(curl_getinfo($h, CURLINFO_TOTAL_TIME));         // double
var_dump(curl_getinfo($h, CURLINFO_TOTAL_TIME_T));       // off_t
var_dump(curl_getinfo($h, CURLINFO_COOKIELIST));         // slist
var_dump(curl_getinfo($h, CURLINFO_CERTINFO));           // a struct, not an slist

// not infos: the DEBUGFUNCTION set, the enum tail, and anything unknown
foreach ([CURLINFO_TEXT, CURLINFO_HEADER_OUT, CURLINFO_DATA_IN, 0, -1, 999999, PHP_INT_MAX, PHP_INT_MIN] as $s) {
    var_dump(curl_getinfo($h, $s));
}
var_dump(curl_getinfo($h, null) === curl_getinfo($h));

// the copies carry the options, and diverge afterwards
$a = curl_init('http://one.example/');
curl_setopt($a, CURLOPT_HTTPHEADER, ['A: 1']);
$b = clone $a;
$c = curl_copy_handle($a);
var_dump(curl_getinfo($b, CURLINFO_EFFECTIVE_URL), curl_getinfo($c, CURLINFO_EFFECTIVE_URL));
curl_setopt($b, CURLOPT_URL, 'http://two.example/');
var_dump(curl_getinfo($a, CURLINFO_EFFECTIVE_URL), curl_getinfo($b, CURLINFO_EFFECTIVE_URL));

// curl_setopt_array applies in order and stops at the first refusal
$g = curl_init();
try { curl_setopt_array($g, [CURLOPT_URL => 'http://applied.example/', 999999 => 1, CURLOPT_USERAGENT => 'zz']); }
catch (Throwable $e) { echo get_class($e), "\n"; }
var_dump(curl_getinfo($g, CURLINFO_EFFECTIVE_URL), curl_errno($g));

try { curl_getinfo($h, 'x'); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { curl_getinfo(); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
$rf = new ReflectionFunction('curl_getinfo');
echo 'curl_getinfo(', implode(', ', array_map(fn($p) => (string)$p->getType() . ' $' . $p->getName()
    . ($p->isDefaultValueAvailable() ? ' = ' . var_export($p->getDefaultValue(), true) : ''), $rf->getParameters())),
    '): ', (string)$rf->getReturnType(), "\n";
--EXPECT--
== NULL keys=41 order=true
  url: string ''
  content_type: NULL NULL
  http_code: integer 0
  header_size: integer 0
  request_size: integer 0
  filetime: integer -1
  ssl_verify_result: integer 0
  redirect_count: integer 0
  total_time: double 0.0
  namelookup_time: double 0.0
  connect_time: double 0.0
  pretransfer_time: double 0.0
  size_upload: double 0.0
  size_download: double 0.0
  speed_download: double 0.0
  speed_upload: double 0.0
  download_content_length: double -1.0
  upload_content_length: double -1.0
  starttransfer_time: double 0.0
  redirect_time: double 0.0
  redirect_url: string ''
  primary_ip: string ''
  certinfo: array array(0)
  primary_port: integer 0
  local_ip: string ''
  local_port: integer 0
  http_version: integer 0
  protocol: integer 0
  ssl_verifyresult: integer 0
  scheme: string ''
  appconnect_time_us: integer 0
  connect_time_us: integer 0
  namelookup_time_us: integer 0
  pretransfer_time_us: integer 0
  redirect_time_us: integer 0
  starttransfer_time_us: integer 0
  total_time_us: integer 0
  effective_method: string 'GET'
  capath: string
  cainfo: string
  conn_id: integer -1
== 'http://example.com/path' keys=41 order=true
  url: string 'http://example.com/path'
  content_type: NULL NULL
  http_code: integer 0
  header_size: integer 0
  request_size: integer 0
  filetime: integer -1
  ssl_verify_result: integer 0
  redirect_count: integer 0
  total_time: double 0.0
  namelookup_time: double 0.0
  connect_time: double 0.0
  pretransfer_time: double 0.0
  size_upload: double 0.0
  size_download: double 0.0
  speed_download: double 0.0
  speed_upload: double 0.0
  download_content_length: double -1.0
  upload_content_length: double -1.0
  starttransfer_time: double 0.0
  redirect_time: double 0.0
  redirect_url: string ''
  primary_ip: string ''
  certinfo: array array(0)
  primary_port: integer 0
  local_ip: string ''
  local_port: integer 0
  http_version: integer 0
  protocol: integer 0
  ssl_verifyresult: integer 0
  scheme: string ''
  appconnect_time_us: integer 0
  connect_time_us: integer 0
  namelookup_time_us: integer 0
  pretransfer_time_us: integer 0
  redirect_time_us: integer 0
  starttransfer_time_us: integer 0
  total_time_us: integer 0
  effective_method: string 'GET'
  capath: string
  cainfo: string
  conn_id: integer -1
== 'https://x.example:8443/a?b=c' keys=41 order=true
  url: string 'https://x.example:8443/a?b=c'
  content_type: NULL NULL
  http_code: integer 0
  header_size: integer 0
  request_size: integer 0
  filetime: integer -1
  ssl_verify_result: integer 0
  redirect_count: integer 0
  total_time: double 0.0
  namelookup_time: double 0.0
  connect_time: double 0.0
  pretransfer_time: double 0.0
  size_upload: double 0.0
  size_download: double 0.0
  speed_download: double 0.0
  speed_upload: double 0.0
  download_content_length: double -1.0
  upload_content_length: double -1.0
  starttransfer_time: double 0.0
  redirect_time: double 0.0
  redirect_url: string ''
  primary_ip: string ''
  certinfo: array array(0)
  primary_port: integer 0
  local_ip: string ''
  local_port: integer 0
  http_version: integer 0
  protocol: integer 0
  ssl_verifyresult: integer 0
  scheme: string ''
  appconnect_time_us: integer 0
  connect_time_us: integer 0
  namelookup_time_us: integer 0
  pretransfer_time_us: integer 0
  redirect_time_us: integer 0
  starttransfer_time_us: integer 0
  total_time_us: integer 0
  effective_method: string 'GET'
  capath: string
  cainfo: string
  conn_id: integer -1
bool(false)
NULL
bool(false)
string(0) ""
string(19) "http://example.com/"
int(0)
float(0)
int(0)
array(0) {
}
array(0) {
}
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(true)
string(19) "http://one.example/"
string(19) "http://one.example/"
string(19) "http://one.example/"
string(19) "http://two.example/"
ValueError
string(23) "http://applied.example/"
int(48)
TypeError: curl_getinfo(): Argument #2 ($option) must be of type ?int, string given
ArgumentCountError: curl_getinfo() expects at least 1 argument, 0 given
curl_getinfo(CurlHandle $handle, ?int $option = NULL): mixed
