--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
clone of a CurlHandle is curl_easy_duphandle(), and so is curl_copy_handle()
--DESCRIPTION--
CurlHandle is the one handle class php lets you clone: `clone $h` maps to
curl_easy_duphandle(), and curl_copy_handle() is the same operation spelled as
a function. Both answer a second, independent handle -- never a second object
over one CURL*, which is what every other handle class here refuses outright.

The OPTION-level half of this (the copy carries the source's options, the two
diverge afterwards, and a copy of a failed handle starts with a clean error
state) is asserted by curl_setopt's own test, which is where the verbs that can
observe an option live. What is observable here is the identity, the type, and
that all three handles keep working independently -- including after the source
is dropped, which is the case that would fault if the copy shared its record.

The error BUFFER is the subtle part and it is asserted there too: libcurl
copies CURLOPT_ERRORBUFFER's value like any other option, so a bare duphandle
leaves the clone writing its failures into the SOURCE's buffer, and into freed
memory once the source is gone.
--SKIPIF--
<?php
if (!extension_loaded('curl')) {
    echo "skip ext/curl not available";
}
?>
--FILE--
<?php
$a = curl_init('http://one.example/');
$b = clone $a;
$c = curl_copy_handle($a);
var_dump(get_class($b), get_class($c));

// two handles are never == each other, only === themselves
var_dump($a === $b, $a == $b, $b == $c, $b === $b);
var_dump(curl_errno($b), curl_error($b), curl_errno($c), curl_error($c));

// each is a handle in its own right
var_dump(curl_reset($b), curl_close($c), curl_errno($a));

// a copy of a copy
$d = clone $c;
var_dump(get_class($d), $d === $c);

// dropping the SOURCE leaves the copies alive: their handles are their own
$a = null;
var_dump(curl_errno($b), curl_errno($c), curl_errno($d));

// close is a no-op, so a closed handle is still copyable
$e = curl_init();
curl_close($e);
var_dump(get_class(clone $e), get_class(curl_copy_handle($e)));

$rf = new ReflectionFunction('curl_copy_handle');
echo 'curl_copy_handle(', implode(', ', array_map(fn($p) => (string)$p->getType() . ' $' . $p->getName(), $rf->getParameters())), '): ', (string)$rf->getReturnType(), "\n";
var_dump((new ReflectionClass('CurlHandle'))->isCloneable());
--EXPECT--
string(10) "CurlHandle"
string(10) "CurlHandle"
bool(false)
bool(false)
bool(false)
bool(true)
int(0)
string(0) ""
int(0)
string(0) ""
NULL
NULL
int(0)
string(10) "CurlHandle"
bool(false)
int(0)
int(0)
int(0)
string(10) "CurlHandle"
string(10) "CurlHandle"
curl_copy_handle(CurlHandle $handle): CurlHandle|false
bool(true)
