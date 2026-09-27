--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A curl_setopt() cast that throws has already written what it answered
--DESCRIPTION--
php casts an option's value with the ordinary string cast, and that cast does
not STOP at an object with no __toString(): it raises the Error and answers an
empty string, and php's own code then sets the option from that answer before
anything unwinds. So the throw and the write both happen, in that order, and a
handler that catches the Error is looking at a handle whose URL is GONE -- not
at one that kept the URL it had.

Skipping the write on a throw is the reading a careful implementation reaches
for, and it is wrong in exactly the way that matters: the caught Error says
"nothing was applied" while php says "the empty string was".

curl_setopt_array stops at the throwing entry, so the options BEFORE it are
applied and the ones after it are not -- and the throwing one is applied too,
as its own empty string. The error state is untouched by any of this: a cast
that throws is not a libcurl failure, so curl_errno stays where the setter's
own entry clear left it.
--SKIPIF--
<?php
if (!extension_loaded('curl')) {
    echo "skip ext/curl not available";
}
?>
--FILE--
<?php
class CurlCastNoString
{
}

$h = curl_init('http://example.com/first');
printf("before: %s\n", var_export(curl_getinfo($h, CURLINFO_EFFECTIVE_URL), true));
try {
    curl_setopt($h, CURLOPT_URL, new CurlCastNoString());
} catch (Throwable $e) {
    printf("caught: %s: %s\n", get_class($e), $e->getMessage());
}
printf("after:  %s errno=%d\n", var_export(curl_getinfo($h, CURLINFO_EFFECTIVE_URL), true),
    curl_errno($h));

/* the same cast, through curl_setopt_array: everything before it is applied */
$a = curl_init('http://example.com/second');
try {
    curl_setopt_array($a, [
        CURLOPT_TIMEOUT => 7,
        CURLOPT_URL => new CurlCastNoString(),
        CURLOPT_USERAGENT => 'never-set',
    ]);
} catch (Throwable $e) {
    printf("array caught: %s: %s\n", get_class($e), $e->getMessage());
}
printf("array after: %s errno=%d\n", var_export(curl_getinfo($a, CURLINFO_EFFECTIVE_URL), true),
    curl_errno($a));

/* a value that casts cleanly is unremarkable, and every scalar has a spelling */
$s = curl_init();
foreach ([42, 1.5, true, false, null] as $v) {
    curl_setopt($s, CURLOPT_URL, $v);
    printf("url from %-7s => %s\n", var_export($v, true),
        var_export(curl_getinfo($s, CURLINFO_EFFECTIVE_URL), true));
}

/* an object WITH __toString() is just a string */
$t = curl_init();
curl_setopt($t, CURLOPT_URL, new class {
    public function __toString(): string
    {
        return 'http://example.com/stringable';
    }
});
printf("stringable => %s\n", var_export(curl_getinfo($t, CURLINFO_EFFECTIVE_URL), true));
?>
--EXPECT--
before: 'http://example.com/first'
caught: Error: Object of class CurlCastNoString could not be converted to string
after:  '' errno=0
array caught: Error: Object of class CurlCastNoString could not be converted to string
array after: '' errno=0
url from 42      => '42'
url from 1.5     => '1.5'
url from true    => '1'
url from false   => ''
url from NULL    => ''
stringable => 'http://example.com/stringable'
--CLEAN--
<?php
unset($h, $a, $s, $t, $v, $e);
?>
