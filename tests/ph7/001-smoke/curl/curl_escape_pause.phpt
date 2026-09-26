--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
curl_escape/curl_unescape are NUL-clean and are not urlencode(); pause answers a code
--DESCRIPTION--
The remainder of the easy-handle surface. Three things here are the library's
answer rather than php's, and none of them is what the familiar php function of
a similar name does:

  * both encoders are called with an explicit LENGTH, so they are NUL-clean in
    both directions -- escaping "\0" answers "%00" and unescaping "%00" answers
    the byte back, where a C-string call would stop at it.
  * `+` is escaped to %2B on the way out and is NOT decoded to a space on the
    way back. This is percent-encoding, not application/x-www-form-urlencoded.
  * an invalid escape passes through unchanged: "%zz", "%" and "%2" are
    themselves.

curl_pause answers libcurl's CURLcode as an INT, not a bool, and leaves the
error state alone -- so a pause on a handle with no transfer reports 43
(CURLE_BAD_FUNCTION_ARGUMENT) while curl_errno() still reports whatever the
last transfer did. curl_upkeep is the third and last verb that CLEARS the error
state, beside the two setters.
--SKIPIF--
<?php
if (!extension_loaded('curl')) {
    echo "skip ext/curl not available";
}
?>
--FILE--
<?php
$h = curl_init();
foreach (['a b/c?d=é&', '', "\0", '%00', 'plain', '~-_.', '+', 'á'] as $s) {
    printf("escape %-14s => %s\n", var_export($s, true), var_export(curl_escape($h, $s), true));
}
foreach (['a%20b%2Fc', '', '%00', '%zz', '%', '%2', 'plain+here', '%c3%a9'] as $s) {
    printf("unescape %-14s => %s\n", var_export($s, true), var_export(curl_unescape($h, $s), true));
}

var_dump(curl_upkeep($h));
foreach ([CURLPAUSE_ALL, CURLPAUSE_CONT, CURLPAUSE_RECV, CURLPAUSE_SEND, 0] as $f) {
    printf("pause %-4d => %s\n", $f, var_export(curl_pause($h, $f), true));
}
printf("errno after pause: %d\n", curl_errno($h));

// which of these clear the handle's error, and which leave it
$e = curl_init('nonsense://x');
curl_setopt($e, CURLOPT_RETURNTRANSFER, true);
curl_exec($e);
$before = curl_errno($e);
curl_escape($e, 'x');
$afterEscape = curl_errno($e);
curl_pause($e, CURLPAUSE_CONT);
$afterPause = curl_errno($e);
curl_upkeep($e);
$afterUpkeep = curl_errno($e);
printf("before=%d escape=%d pause=%d upkeep=%d\n", $before, $afterEscape, $afterPause, $afterUpkeep);

foreach (['curl_escape', 'curl_unescape', 'curl_upkeep', 'curl_pause'] as $f) {
    $rf = new ReflectionFunction($f);
    echo $f, '(', implode(', ', array_map(fn($p) => (string)$p->getType() . ' $' . $p->getName(), $rf->getParameters())),
        '): ', (string)$rf->getReturnType(), "\n";
}
try { curl_escape($h, ['a']); } catch (Throwable $x) { echo get_class($x), ': ', $x->getMessage(), "\n"; }
try { curl_pause($h); } catch (Throwable $x) { echo get_class($x), ': ', $x->getMessage(), "\n"; }
--EXPECT--
escape 'a b/c?d=é&'  => 'a%20b%2Fc%3Fd%3D%C3%A9%26'
escape ''             => ''
escape '' . "\0" . '' => '%00'
escape '%00'          => '%2500'
escape 'plain'        => 'plain'
escape '~-_.'         => '~-_.'
escape '+'            => '%2B'
escape 'á'           => '%C3%A1'
unescape 'a%20b%2Fc'    => 'a b/c'
unescape ''             => ''
unescape '%00'          => '' . "\0" . ''
unescape '%zz'          => '%zz'
unescape '%'            => '%'
unescape '%2'           => '%2'
unescape 'plain+here'   => 'plain+here'
unescape '%c3%a9'       => 'é'
bool(true)
pause 5    => 43
pause 0    => 43
pause 1    => 43
pause 4    => 43
pause 0    => 43
errno after pause: 0
before=1 escape=1 pause=1 upkeep=0
curl_escape(CurlHandle $handle, string $string): string|false
curl_unescape(CurlHandle $handle, string $string): string|false
curl_upkeep(CurlHandle $handle): bool
curl_pause(CurlHandle $handle, int $flags): int
TypeError: curl_escape(): Argument #2 ($string) must be of type string, array given
ArgumentCountError: curl_pause() expects exactly 2 arguments, 1 given
