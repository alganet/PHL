--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
curl_setopt() converts by option KIND, and refuses an option php's own switch does not know
--DESCRIPTION--
The type half of curl_setopt, derived by sweeping all 269 options against 17
value types and reading what php answered. Four kinds are covered here -- the
long options, the string options, the ten that take an array php turns into a
curl_slist, and php's own CURLOPT_SAFE_UPLOAD. The callback, stream and
POSTFIELDS kinds refuse loudly until their slices land.

What the sweep decided, and what a reading of the manual would not:

  * php does not ask libcurl whether an option EXISTS. An unknown number falls
    off the end of php's own switch, so it never reaches the library -- and the
    handle still records CURLE_UNKNOWN_OPTION, so a caught ValueError leaves
    curl_errno() at 48 with libcurl's own text for it.
  * a string option takes ANY value: null becomes "", an array becomes "Array"
    with the ordinary conversion warning, an object with no __toString() is the
    ordinary Error. What php then screens is the RESULT, for a NUL byte, with
    the same sentence curl_init() uses -- and a slist ELEMENT carrying a NUL is
    NOT screened.
  * CURLOPT_SSL_VERIFYHOST is the one option php hand-checks: the value 1 is an
    E_NOTICE and 2 is used instead.
  * curl_setopt_array applies IN ORDER and stops at the first refusal, and its
    two ValueErrors are different sentences -- one for a key that is not a
    valid option, one for a key that is not an option at all.
--SKIPIF--
<?php
if (!extension_loaded('curl')) {
    echo "skip ext/curl not available";
}
?>
--FILE--
<?php
$w = '';
set_error_handler(function ($no, $msg) use (&$w) { $w .= "[$no] $msg\n"; return true; });
// The smoke corpus shares ONE interpreter, so a top-level function name has to
// be unique across the whole of it: a plain `t()` collides with the one in
// function/round/round_edge_and_errors.phpt and the second declaration fatals.
function curlSetoptCase(string $label, callable $fn) {
    global $w;
    $w = '';
    try { $r = $fn(); echo $label, ' => ', var_export($r, true), "\n"; }
    catch (Throwable $e) { echo $label, ' => ', get_class($e), ': ', $e->getMessage(), "\n"; }
    if ($w !== '') echo '   warn: ', str_replace("\n", ' | ', trim($w)), "\n";
}
$h = curl_init();
echo "== string options ==\n";
curlSetoptCase('URL nul',        fn() => curl_setopt($h, CURLOPT_URL, "http://a\0b/"));
curlSetoptCase('USERAGENT nul',  fn() => curl_setopt($h, CURLOPT_USERAGENT, "a\0b"));
curlSetoptCase('URL null',       fn() => curl_setopt($h, CURLOPT_URL, null));
curlSetoptCase('URL array',      fn() => curl_setopt($h, CURLOPT_URL, ['x']));
curlSetoptCase('URL int',        fn() => curl_setopt($h, CURLOPT_URL, 42));
curlSetoptCase('URL true',       fn() => curl_setopt($h, CURLOPT_URL, true));
curlSetoptCase('URL object',     fn() => curl_setopt($h, CURLOPT_URL, new stdClass));

echo "== long options ==\n";
curlSetoptCase('TIMEOUT float',  fn() => curl_setopt($h, CURLOPT_TIMEOUT, 1.9));
curlSetoptCase('TIMEOUT "x"',    fn() => curl_setopt($h, CURLOPT_TIMEOUT, 'x'));
curlSetoptCase('TIMEOUT null',   fn() => curl_setopt($h, CURLOPT_TIMEOUT, null));
curlSetoptCase('TIMEOUT array',  fn() => curl_setopt($h, CURLOPT_TIMEOUT, ['a', 'b']));
curlSetoptCase('TIMEOUT obj',    fn() => curl_setopt($h, CURLOPT_TIMEOUT, new stdClass));
curlSetoptCase('VERIFYHOST 1',   fn() => curl_setopt($h, CURLOPT_SSL_VERIFYHOST, 1));
curlSetoptCase('VERIFYHOST "1"', fn() => curl_setopt($h, CURLOPT_SSL_VERIFYHOST, '1'));
curlSetoptCase('VERIFYHOST 2',   fn() => curl_setopt($h, CURLOPT_SSL_VERIFYHOST, 2));

echo "== an option php does not know ==\n";
curlSetoptCase('option 999999',  fn() => curl_setopt($h, 999999, 1));
curlSetoptCase('option -5',      fn() => curl_setopt($h, -5, 1));
curlSetoptCase('option 0',       fn() => curl_setopt($h, 0, 1));
printf("   errno after the ValueError: %d %s\n", curl_errno($h), var_export(curl_error($h), true));
curl_setopt($h, CURLOPT_TIMEOUT, 3);
printf("   a good setopt clears it: %d %s\n", curl_errno($h), var_export(curl_error($h), true));

echo "== slist options ==\n";
curlSetoptCase('HTTPHEADER list',   fn() => curl_setopt($h, CURLOPT_HTTPHEADER, ['A: 1', 'B: 2']));
curlSetoptCase('HTTPHEADER empty',  fn() => curl_setopt($h, CURLOPT_HTTPHEADER, []));
curlSetoptCase('HTTPHEADER keys',   fn() => curl_setopt($h, CURLOPT_HTTPHEADER, ['k' => 'A: 1', 9 => 'B: 2']));
curlSetoptCase('HTTPHEADER ints',   fn() => curl_setopt($h, CURLOPT_HTTPHEADER, [1, 2.5, true, null]));
curlSetoptCase('HTTPHEADER nested', fn() => curl_setopt($h, CURLOPT_HTTPHEADER, [['a']]));
curlSetoptCase('HTTPHEADER obj',    fn() => curl_setopt($h, CURLOPT_HTTPHEADER, [new stdClass]));
curlSetoptCase('HTTPHEADER nul',    fn() => curl_setopt($h, CURLOPT_HTTPHEADER, ["A: \0"]));
curlSetoptCase('HTTPHEADER string', fn() => curl_setopt($h, CURLOPT_HTTPHEADER, 'A: 1'));
curlSetoptCase('RESOLVE list',      fn() => curl_setopt($h, CURLOPT_RESOLVE, ['a.example:80:127.0.0.1']));
// re-setting replaces the previous list rather than appending to it
curlSetoptCase('HTTPHEADER again',  fn() => curl_setopt($h, CURLOPT_HTTPHEADER, ['C: 3']));

echo "== php's own SAFE_UPLOAD ==\n";
foreach ([true, false, 1, 0, 'x', null] as $v) {
    curlSetoptCase('SAFE_UPLOAD ' . var_export($v, true), fn() => curl_setopt($h, CURLOPT_SAFE_UPLOAD, $v));
}

echo "== setopt_array ==\n";
curlSetoptCase('all good',   fn() => curl_setopt_array($h, [CURLOPT_TIMEOUT => 5, CURLOPT_URL => 'http://x/']));
curlSetoptCase('bad option', fn() => curl_setopt_array($h, [CURLOPT_TIMEOUT => 5, 999999 => 1]));
curlSetoptCase('string key', fn() => curl_setopt_array($h, ['CURLOPT_URL' => 'http://x/']));
curlSetoptCase('empty',      fn() => curl_setopt_array($h, []));
curlSetoptCase('not array',  fn() => curl_setopt_array($h, 'x'));

// The smoke corpus shares ONE interpreter: an error handler left installed
// here swallows every later test's warnings.
restore_error_handler();

foreach (['curl_setopt', 'curl_setopt_array'] as $f) {
    $rf = new ReflectionFunction($f);
    echo $f, '(', implode(', ', array_map(fn($p) => (string)$p->getType() . ' $' . $p->getName(), $rf->getParameters())),
        '): ', (string)$rf->getReturnType(), "\n";
}
--EXPECT--
== string options ==
URL nul => ValueError: curl_setopt(): cURL option must not contain any null bytes
USERAGENT nul => ValueError: curl_setopt(): cURL option must not contain any null bytes
URL null => true
URL array => true
   warn: [2] Array to string conversion
URL int => true
URL true => true
URL object => Error: Object of class stdClass could not be converted to string
== long options ==
TIMEOUT float => true
TIMEOUT "x" => true
TIMEOUT null => true
TIMEOUT array => true
TIMEOUT obj => true
   warn: [2] Object of class stdClass could not be converted to int
VERIFYHOST 1 => true
   warn: [8] curl_setopt(): CURLOPT_SSL_VERIFYHOST no longer accepts the value 1, value 2 will be used instead
VERIFYHOST "1" => true
   warn: [8] curl_setopt(): CURLOPT_SSL_VERIFYHOST no longer accepts the value 1, value 2 will be used instead
VERIFYHOST 2 => true
== an option php does not know ==
option 999999 => ValueError: curl_setopt(): Argument #2 ($option) is not a valid cURL option
option -5 => ValueError: curl_setopt(): Argument #2 ($option) is not a valid cURL option
option 0 => ValueError: curl_setopt(): Argument #2 ($option) is not a valid cURL option
   errno after the ValueError: 48 'An unknown option was passed in to libcurl'
   a good setopt clears it: 0 ''
== slist options ==
HTTPHEADER list => true
HTTPHEADER empty => true
HTTPHEADER keys => true
HTTPHEADER ints => true
HTTPHEADER nested => true
   warn: [2] Array to string conversion
HTTPHEADER obj => Error: Object of class stdClass could not be converted to string
HTTPHEADER nul => true
HTTPHEADER string => TypeError: curl_setopt(): The CURLOPT_HTTPHEADER option must have an array value
RESOLVE list => true
HTTPHEADER again => true
== php's own SAFE_UPLOAD ==
SAFE_UPLOAD true => true
SAFE_UPLOAD false => ValueError: curl_setopt(): Disabling safe uploads is no longer supported
SAFE_UPLOAD 1 => true
SAFE_UPLOAD 0 => ValueError: curl_setopt(): Disabling safe uploads is no longer supported
SAFE_UPLOAD 'x' => true
SAFE_UPLOAD NULL => ValueError: curl_setopt(): Disabling safe uploads is no longer supported
== setopt_array ==
all good => true
bad option => ValueError: curl_setopt_array(): Argument #2 ($options) must contain only valid cURL options
string key => ValueError: curl_setopt_array(): Argument #2 ($options) contains an invalid cURL option
empty => true
not array => TypeError: curl_setopt_array(): Argument #2 ($options) must be of type array, string given
curl_setopt(CurlHandle $handle, int $option, mixed $value): bool
curl_setopt_array(CurlHandle $handle, array $options): bool
