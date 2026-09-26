--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
var_dump of a CurlHandle shows an empty object, and the handle number is the object's
--DESCRIPTION--
The one part of the handle's presentation that cannot live in the smoke corpus:
var_dump prints an object HANDLE, and the shared in-process interpreter's
counter has already been moved by every earlier test. Here the process is the
test's own, so the numbers are the ones php prints.

php declares no property on CurlHandle, so the storage this engine keeps on the
instance (the libcurl handle) must appear on NO surface -- which is what the
empty parentheses are.
--SKIPIF--
<?php
if (!extension_loaded('curl')) {
    echo "skip ext/curl not available";
}
?>
--FILE--
<?php
$a = curl_init();
$b = curl_init('http://example.com/');
var_dump($a);
var_dump($b);
var_dump([$a, $b]);
var_dump($a === $a, $a === $b, $a == $b);
--EXPECT--
object(CurlHandle)#1 (0) {
}
object(CurlHandle)#2 (0) {
}
array(2) {
  [0]=>
  object(CurlHandle)#1 (0) {
  }
  [1]=>
  object(CurlHandle)#2 (0) {
  }
}
bool(true)
bool(false)
bool(false)
