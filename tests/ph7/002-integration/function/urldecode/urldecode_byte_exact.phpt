--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
urldecode/rawurldecode: byte-exact, and rawurldecode leaves '+' alone
--DESCRIPTION--
rawurldecode() was an alias of urldecode(), so it turned every literal '+' into a
space, and the shared decoder tried to interpret %XX sequences as UTF-8: a byte
>= 0xC0 was folded with its continuation bytes and re-encoded, a truncated or
non-hex escape was dropped or read as zero. php just walks bytes -- "%" plus two
hex digits is that byte, everything else is copied through. parse_str() and the
query-string/cookie parsers ride the same decoder.
--FILE--
<?php
$cases = ['a+b', 'a%20b', 'abc%', 'abc%4', 'a%zzb', 'a%2', '%C3%A9', '%C3', '%FF',
          '%C3%28', '%ED%A0%80', '%00', 'a%2Bb', '%2b', 'caf%C3%A9+bar', '%%41',
          '%41%', '%7E', 'x%', '%', '+', '%zz', '%C2%80', '%F0%9F%98%80'];
foreach ($cases as $s) {
    printf("%-14s ud=%-14s rud=%s\n", $s, bin2hex(urldecode($s)), bin2hex(rawurldecode($s)));
}
var_dump(urldecode(''), rawurldecode(''));

// parse_str() decodes keys and values through the same routine.
parse_str('a=%FF&b=%C3&c=x%&d=a+b&e=%zz&f%20g=1', $r);
foreach ($r as $k => $v) {
    printf("%s=%s ", bin2hex($k), bin2hex($v));
}
echo "\n";

// Round trip: every byte survives rawurlencode/rawurldecode and urlencode/urldecode.
$all = '';
for ($i = 0; $i < 256; $i++) {
    $all .= chr($i);
}
var_dump(rawurldecode(rawurlencode($all)) === $all, urldecode(urlencode($all)) === $all);
?>
--EXPECT--
a+b            ud=612062         rud=612b62
a%20b          ud=612062         rud=612062
abc%           ud=61626325       rud=61626325
abc%4          ud=6162632534     rud=6162632534
a%zzb          ud=61257a7a62     rud=61257a7a62
a%2            ud=612532         rud=612532
%C3%A9         ud=c3a9           rud=c3a9
%C3            ud=c3             rud=c3
%FF            ud=ff             rud=ff
%C3%28         ud=c328           rud=c328
%ED%A0%80      ud=eda080         rud=eda080
%00            ud=00             rud=00
a%2Bb          ud=612b62         rud=612b62
%2b            ud=2b             rud=2b
caf%C3%A9+bar  ud=636166c3a920626172 rud=636166c3a92b626172
%%41           ud=2541           rud=2541
%41%           ud=4125           rud=4125
%7E            ud=7e             rud=7e
x%             ud=7825           rud=7825
%              ud=25             rud=25
+              ud=20             rud=2b
%zz            ud=257a7a         rud=257a7a
%C2%80         ud=c280           rud=c280
%F0%9F%98%80   ud=f09f9880       rud=f09f9880
string(0) ""
string(0) ""
61=ff 62=c3 63=7825 64=612062 65=257a7a 665f67=31 
bool(true)
bool(true)
--CLEAN--
<?php
