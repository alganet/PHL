--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
(int) on a curl easy/multi handle is its object handle, and says nothing
--DESCRIPTION--
php has no `__toInt()` and casting an object to int is a warning and a 1 —
except for the two classes it gives a cast handler to, and its own source says
why: `CurlHandle` and `CurlMultiHandle` used to be RESOURCES, whose `(int)` was
the resource id, so a program that keyed a table by it had to keep working.
Composer's CurlDownloader is that program: it indexes its job table by
`(int) $curlHandle`, and with Composer's handler promoting warnings to
exceptions every download died on the diagnostic.

The cast is the only door that moves. `(float)`, `(string)` and every other
class — the SHARE handle beside them, a PDO connection — keep php's refusal,
because php gave the handler to exactly two classes and only for `IS_LONG`.
--SKIPIF--
<?php
if (!extension_loaded('curl')) {
    echo "skip ext/curl not available";
} elseif (!class_exists('PDO')) {
    echo "skip ext/pdo not available";
}
?>
--FILE--
<?php
$ch = curl_init();
$cm = curl_multi_init();
$cs = curl_share_init();
$pdo = new PDO('sqlite::memory:');

echo "curl-easy int is its handle: ", var_export((int)$ch === spl_object_id($ch), true), "\n";
echo "curl-multi int is its handle: ", var_export((int)$cm === spl_object_id($cm), true), "\n";
echo "intval agrees: ", var_export(intval($ch) === (int)$ch, true), "\n";
echo "printf %d agrees: ", var_export(sprintf('%d', $ch) === (string)(int)$ch, true), "\n";
$copy = $ch;
settype($copy, 'integer');
echo "settype agrees: ", var_export($copy === (int)$ch, true), "\n";
echo "two handles differ: ", var_export((int)$ch !== (int)$cm, true), "\n";

set_error_handler(function ($no, $str) { echo "  diagnostic: $str\n"; return true; });
echo "share handle:\n";
var_dump((int)$cs);
echo "pdo:\n";
var_dump((int)$pdo);
echo "easy handle as float:\n";
var_dump((float)$ch);
restore_error_handler();
echo "easy handle as string: ";
try {
    echo (string)$ch;
} catch (Throwable $e) {
    echo get_class($e), ': ', $e->getMessage();
}
echo "\n";
?>
--EXPECT--
curl-easy int is its handle: true
curl-multi int is its handle: true
intval agrees: true
printf %d agrees: true
settype agrees: true
two handles differ: true
share handle:
  diagnostic: Object of class CurlShareHandle could not be converted to int
int(1)
pdo:
  diagnostic: Object of class PDO could not be converted to int
int(1)
easy handle as float:
  diagnostic: Object of class CurlHandle could not be converted to float
float(1)
easy handle as string: Error: Object of class CurlHandle could not be converted to string
