--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An opaque native handle is UNCOMPARABLE with everything but itself
--DESCRIPTION--
The other face of the curl cast, and a different php handler: `compare`. php
gives one to every object that stands for something outside the engine — the
three curl handles, a PDO connection, a statement, a lazy row — and it
recognizes NOTHING, so every comparison but the identity shortcut is
ZEND_UNCOMPARABLE: `==`, `<` and `>` are all false and `<=>` is 1 from either
direction, silently.

Without it these fell through to php's cast-the-object rule, which warns
`could not be converted to int` and then calls the handle equal to 1 — so
`in_array($ch, [1,2,3])` was TRUE, `array_search` answered 0, and every one of
them printed a diagnostic php does not. A BOOL partner is the one thing the
handler declines: php decides an object against `true`/`false` by the cast, and
an object is truthy.
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
$other = curl_init();
$pdo = new PDO('sqlite::memory:');
$pdo->exec('CREATE TABLE t(a)');
$st = $pdo->query('SELECT 1');

set_error_handler(function ($no, $str) { echo "  diagnostic: $str\n"; return true; });
foreach (['CurlHandle' => $ch, 'PDO' => $pdo, 'PDOStatement' => $st] as $name => $o) {
    echo "== $name\n";
    var_dump($o == 1, $o == 0, $o < 2, $o > 0, $o <=> 1, $o == "2", $o == 2.0, $o == null, $o == []);
    var_dump($o == true, $o == false);
    var_dump($o == $o, $o === $o);
}
echo "== two handles\n";
var_dump($ch == $other, $ch === $other, $ch <=> $other);
echo "== through the array doors\n";
var_dump(in_array($ch, [1, 2, 3]), array_search($ch, [1, 2, 3]), max(1, $ch) === 1);
switch ($ch) {
    case 1:
        echo "case1\n";
        break;
    default:
        echo "default\n";
}
?>
--EXPECT--
== CurlHandle
bool(false)
bool(false)
bool(false)
bool(false)
int(1)
bool(false)
bool(false)
bool(false)
bool(false)
bool(true)
bool(false)
bool(true)
bool(true)
== PDO
bool(false)
bool(false)
bool(false)
bool(false)
int(1)
bool(false)
bool(false)
bool(false)
bool(false)
bool(true)
bool(false)
bool(true)
bool(true)
== PDOStatement
bool(false)
bool(false)
bool(false)
bool(false)
int(1)
bool(false)
bool(false)
bool(false)
bool(false)
bool(true)
bool(false)
bool(true)
bool(true)
== two handles
bool(false)
bool(false)
int(1)
== through the array doors
bool(false)
bool(false)
bool(true)
default
