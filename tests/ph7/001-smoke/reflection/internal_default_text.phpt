--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An internal parameter's declared default is carried, escapes and all
--DESCRIPTION--
Twelve internal parameters had no default row at all -- the signature table
spelled them `= ?`, its marker for "optional, nothing printable" -- so
isDefaultValueAvailable() answered false where php answers true and
getDefaultValue() raised. Four of them could not be written any other way:
a signature is a C string and trim()'s ` \n\r\t\v\x00` ends in a NUL, so the
reader had to learn php's escapes -- the same set the exporter already puts
back when it prints the line.
--FILE--
<?php
function idtShow($label, $fn) {
    try { $out = $fn(); }
    catch (Throwable $e) { echo $label, ' => ', get_class($e), ': ', $e->getMessage(), "\n"; return; }
    echo $label, ' => ', $out, "\n";
}
$idtRows = [['trim', 1], ['ltrim', 1], ['rtrim', 1], ['chop', 1], ['ucwords', 1],
            ['chunk_split', 2], ['wordwrap', 2], ['getopt', 1], ['password_hash', 2],
            ['password_needs_rehash', 2], ['unserialize', 1], ['array_splice', 3]];
foreach ($idtRows as [$idtName, $idtPos]) {
    $idtParam = (new ReflectionFunction($idtName))->getParameters()[$idtPos];
    idtShow("$idtName #$idtPos avail", fn() => (int)$idtParam->isDefaultValueAvailable());
    idtShow("$idtName #$idtPos bytes", function () use ($idtParam) {
        $v = $idtParam->getDefaultValue();
        return is_array($v) ? 'array(' . count($v) . ')' : bin2hex($v);
    });
    idtShow("$idtName #$idtPos line", fn() => (string)$idtParam);
}
/* The escapes read back as the bytes php holds, and print as php prints them. */
var_dump(trim("\x00 xy \t\v\r\n") === 'xy');
var_dump((new ReflectionFunction('trim'))->getParameters()[1]->getDefaultValue()
         === " \n\r\t\v\x00");
--EXPECT--
trim #1 avail => 1
trim #1 bytes => 200a0d090b00
trim #1 line => Parameter #1 [ <optional> string $characters = " \n\r\t\v\x00" ]
ltrim #1 avail => 1
ltrim #1 bytes => 200a0d090b00
ltrim #1 line => Parameter #1 [ <optional> string $characters = " \n\r\t\v\x00" ]
rtrim #1 avail => 1
rtrim #1 bytes => 200a0d090b00
rtrim #1 line => Parameter #1 [ <optional> string $characters = " \n\r\t\v\x00" ]
chop #1 avail => 1
chop #1 bytes => 200a0d090b00
chop #1 line => Parameter #1 [ <optional> string $characters = " \n\r\t\v\x00" ]
ucwords #1 avail => 1
ucwords #1 bytes => 20090d0a0c0b
ucwords #1 line => Parameter #1 [ <optional> string $separators = " \t\r\n\f\v" ]
chunk_split #2 avail => 1
chunk_split #2 bytes => 0d0a
chunk_split #2 line => Parameter #2 [ <optional> string $separator = "\r\n" ]
wordwrap #2 avail => 1
wordwrap #2 bytes => 0a
wordwrap #2 line => Parameter #2 [ <optional> string $break = "\n" ]
getopt #1 avail => 1
getopt #1 bytes => array(0)
getopt #1 line => Parameter #1 [ <optional> array $long_options = [] ]
password_hash #2 avail => 1
password_hash #2 bytes => array(0)
password_hash #2 line => Parameter #2 [ <optional> array $options = [] ]
password_needs_rehash #2 avail => 1
password_needs_rehash #2 bytes => array(0)
password_needs_rehash #2 line => Parameter #2 [ <optional> array $options = [] ]
unserialize #1 avail => 1
unserialize #1 bytes => array(0)
unserialize #1 line => Parameter #1 [ <optional> array $options = [] ]
array_splice #3 avail => 1
array_splice #3 bytes => array(0)
array_splice #3 line => Parameter #3 [ <optional> mixed $replacement = [] ]
bool(true)
bool(true)
