--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A resource compares as its ID, not as a boolean
--FILE--
<?php
/* php compares a resource with a NON-resource as its ID -- the number
 * `(int)$fp` answers -- and the other side takes php's LEGACY scalar-to-number
 * conversion, so a non-numeric string is 0 there rather than being compared as
 * a string. PHL compared the pair as BOOLEANS, so an open resource equalled
 * every non-empty string, every non-zero number and every other open resource,
 * and `max($fp, $id + 5)` answered the resource.
 *
 * The ID itself is the engine's own -- php's CLI has spent a few before the
 * script runs, and the in-process test runner spends thousands -- so every row
 * is written against `(int)$fp` and none of them prints it. */
$fp = fopen('php://memory', 'r');
$other = fopen('php://memory', 'r');
$id = (int)$fp;
$cases = [
    'id - 1'        => $id - 1,
    'id'            => $id,
    'id + 1'        => $id + 1,
    '0'             => 0,
    '-1'            => -1,
    '(string)id'    => (string)$id,
    'id . "abc"'    => $id . 'abc',
    '" id "'        => " $id ",
    '"x"'           => 'x',
    '""'            => '',
    '"0"'           => '0',
    'null'          => null,
    'true'          => true,
    'false'         => false,
    '[]'            => [],
    '[1]'           => [1],
    'other stream'  => $other,
    'same stream'   => $fp,
    'id + 0.5'      => $id + 0.5,
    'id - 0.5'      => $id - 0.5,
    'NAN'           => NAN,
    'INF'           => INF,
    '-INF'          => -INF,
    'new stdClass'  => new stdClass,
];
foreach ($cases as $label => $v) {
    printf("%-14s == %-5s < %-5s > %-5s <=> %2d | flipped < %-5s > %-5s <=> %2d\n",
        $label, var_export($fp == $v, true), var_export($fp < $v, true),
        var_export($fp > $v, true), $fp <=> $v,
        var_export($v < $fp, true), var_export($v > $fp, true), $v <=> $fp);
}
/* The same ID answers every ordering surface, none of which has an operator of
 * its own. */
var_dump(in_array($id, [$fp]), in_array("$id", [$fp]), in_array($fp, [$id]),
         array_search($fp, [$id]), max($fp, $id + 5) === $id + 5,
         min($fp, $id - 5) === $id - 5, $fp == $other, $fp === $fp);
/* Sorted against NUMBERS, where the comparison is a consistent ordering: the
 * stream lands on its own ID. (A non-numeric string mixed in would not be --
 * php compares one against an int as a STRING but against a resource as 0, so
 * the order there is the sort algorithm's, not the comparison's.) */
$sorted = [$id + 1, $fp, $id - 1, $id + 2];
usort($sorted, fn ($a, $b) => $a <=> $b);
echo implode(',', array_map(fn ($v) => is_resource($v) ? 'STREAM'
    : ($v === $id + 1 ? 'id+1' : ($v === $id + 2 ? 'id+2' : 'id-1')), $sorted)), "\n";
fclose($fp);
fclose($other);
?>
--EXPECT--
id - 1         == false < false > true  <=>  1 | flipped < true  > false <=> -1
id             == true  < false > false <=>  0 | flipped < false > false <=>  0
id + 1         == false < true  > false <=> -1 | flipped < false > true  <=>  1
0              == false < false > true  <=>  1 | flipped < true  > false <=> -1
-1             == false < false > true  <=>  1 | flipped < true  > false <=> -1
(string)id     == true  < false > false <=>  0 | flipped < false > false <=>  0
id . "abc"     == true  < false > false <=>  0 | flipped < false > false <=>  0
" id "         == true  < false > false <=>  0 | flipped < false > false <=>  0
"x"            == false < false > true  <=>  1 | flipped < true  > false <=> -1
""             == false < false > true  <=>  1 | flipped < true  > false <=> -1
"0"            == false < false > true  <=>  1 | flipped < true  > false <=> -1
null           == false < false > true  <=>  1 | flipped < true  > false <=> -1
true           == true  < false > false <=>  0 | flipped < false > false <=>  0
false          == false < false > true  <=>  1 | flipped < true  > false <=> -1
[]             == false < true  > false <=> -1 | flipped < false > true  <=>  1
[1]            == false < true  > false <=> -1 | flipped < false > true  <=>  1
other stream   == false < true  > false <=> -1 | flipped < false > true  <=>  1
same stream    == true  < false > false <=>  0 | flipped < false > false <=>  0
id + 0.5       == false < true  > false <=> -1 | flipped < false > true  <=>  1
id - 0.5       == false < false > true  <=>  1 | flipped < true  > false <=> -1
NAN            == false < false > false <=>  1 | flipped < false > false <=>  1
INF            == false < true  > false <=> -1 | flipped < false > true  <=>  1
-INF           == false < false > true  <=>  1 | flipped < true  > false <=> -1
new stdClass   == false < true  > false <=> -1 | flipped < false > true  <=>  1
bool(true)
bool(true)
bool(true)
int(0)
bool(true)
bool(true)
bool(false)
bool(true)
id-1,STREAM,id+1,id+2
