--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A malformed O:/E: header reports php's offset, not the token's start
--DESCRIPTION--
The s: reader already knew php's rule — past the DECLARED length php stops blaming
the token and reports where the declaration turned out to be WRONG — and the
object and enum headers never had it, so every malformed header answered "offset
0" whatever was actually wrong with it. Newly worth fixing because an unknown
class no longer abandons the parse: a header that fails now really is malformed,
and the offset is the only thing saying where. The rules, probe-verified: a length
that overruns the buffer (or an EMPTY class name, which php refuses outright)
blames the length DIGITS; a length that merely disagrees with the payload blames
the byte where the closing quote should have been; past the name php reports where
the parser actually stopped, with a NEGATIVE count read before being rejected so
the blame falls past its digits; and anything malformed BEFORE the name is still
the token's own start. An unknown ENUM is named first — `Class 'X' not found` —
because a case identity has no incomplete-object stand-in the way an object does.
Recorded divergence: an object header truncated EXACTLY where the count belongs
(`O:2:"Ab":`) takes an extra `Bad unserialize data` warning in php and reports the
class name's end rather than the EOF; PHL reports the EOF and says it once.
--FILE--
<?php
set_error_handler(function ($n, $s) { echo '  W: ', $s, "\n"; return true; });
foreach ([
    /* The declared length OVERRUNS the buffer, or is empty (php refuses an empty
     * class name): both blame the length DIGITS. */
    'O:9:"Ab":0:{}',
    'O:0:"":0:{}',
    'E:9:"Ab:Cd";',
    'E:0:"";',
    /* The length merely DISAGREES with the payload: the byte where the closing
     * quote should have been. */
    'O:3:"Nope":1:{s:1:"z";i:7;}',
    'E:3:"Ab:Cd";',
    'O:2:"Ab"',
    /* Past the name php reports where the parser actually stopped. A negative
     * count is read and then rejected, so the blame falls past its digits. */
    'O:2:"Ab":1',
    'O:2:"Ab":x:{}',
    'O:2:"Ab":1:',
    'O:2:"Ab":1:{',
    'O:2:"Ab":1:{s:1:"a";',
    'O:2:"Ab":-1:{}',
    'E:5:"Ab:Cd"',
    /* Malformed before the name is still the TOKEN's own start. */
    'O:x:"Ab":0:{}',
    'O:2:Ab":0:{}',
    'O:-2:"Ab":0:{}',
    /* A container's short count keeps its own two-diagnostic shape. */
    'O:2:"Ab":2:{s:1:"a";i:1;}',
    /* An unknown ENUM is named before the offset — a case identity has no
     * incomplete-object stand-in the way an object does. */
    'E:9:"NoSuch:C";',
] as $offP) {
    echo str_pad(var_export($offP, true), 30), " ->\n";
    var_dump(unserialize($offP));
}
--EXPECT--
'O:9:"Ab":0:{}'                ->
  W: unserialize(): Error at offset 2 of 13 bytes
bool(false)
'O:0:"":0:{}'                  ->
  W: unserialize(): Error at offset 2 of 11 bytes
bool(false)
'E:9:"Ab:Cd";'                 ->
  W: unserialize(): Error at offset 2 of 12 bytes
bool(false)
'E:0:"";'                      ->
  W: unserialize(): Error at offset 2 of 7 bytes
bool(false)
'O:3:"Nope":1:{s:1:"z";i:7;}'  ->
  W: unserialize(): Error at offset 8 of 27 bytes
bool(false)
'E:3:"Ab:Cd";'                 ->
  W: unserialize(): Error at offset 8 of 12 bytes
bool(false)
'O:2:"Ab"'                     ->
  W: unserialize(): Error at offset 8 of 8 bytes
bool(false)
'O:2:"Ab":1'                   ->
  W: unserialize(): Error at offset 10 of 10 bytes
bool(false)
'O:2:"Ab":x:{}'                ->
  W: unserialize(): Error at offset 9 of 13 bytes
bool(false)
'O:2:"Ab":1:'                  ->
  W: unserialize(): Error at offset 11 of 11 bytes
bool(false)
'O:2:"Ab":1:{'                 ->
  W: unserialize(): Error at offset 12 of 12 bytes
bool(false)
'O:2:"Ab":1:{s:1:"a";'         ->
  W: unserialize(): Error at offset 20 of 20 bytes
bool(false)
'O:2:"Ab":-1:{}'               ->
  W: unserialize(): Error at offset 11 of 14 bytes
bool(false)
'E:5:"Ab:Cd"'                  ->
  W: unserialize(): Error at offset 11 of 11 bytes
bool(false)
'O:x:"Ab":0:{}'                ->
  W: unserialize(): Error at offset 0 of 13 bytes
bool(false)
'O:2:Ab":0:{}'                 ->
  W: unserialize(): Error at offset 0 of 12 bytes
bool(false)
'O:-2:"Ab":0:{}'               ->
  W: unserialize(): Error at offset 0 of 14 bytes
bool(false)
'O:2:"Ab":2:{s:1:"a";i:1;}'    ->
  W: unserialize(): Unexpected end of serialized data
  W: unserialize(): Error at offset 24 of 25 bytes
bool(false)
'E:9:"NoSuch:C";'              ->
  W: unserialize(): Error at offset 14 of 15 bytes
bool(false)
