--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: date()/DateTime::format() token matrix on a fixed timestamp
--FILE--
<?php
// 2024-01-15 10:30:45 UTC
$ts = 1705314645;
foreach (str_split('dDjlNSwzWFmMntLoXxYyaABgGhHisuveIOPpTZcrU') as $t) {
    echo $t, '=', date($t, $ts), "\n";
}
$d = new DateTime('2024-01-15 10:30:45', new DateTimeZone('+05:30'));
echo $d->format('e|T|P|p|O|Z|c|r|U'), "\n";
echo $d->format(DATE_ATOM), "\n";
// noon/midnight 12-hour edges
echo date('a A g h', mktime(12, 0, 0, 1, 1, 2024)), "\n";
echo date('a A g h', mktime(0, 30, 0, 1, 1, 2024)), "\n";
// ISO week-year boundaries
echo date('o-\WW', mktime(0, 0, 0, 12, 31, 2024)), "\n";
echo date('o-\WW', mktime(0, 0, 0, 1, 1, 2027)), "\n";
// year padding
echo (new DateTime('0070-03-01', new DateTimeZone('UTC')))->format('Y|y|o|X|x'), "\n";
?>
--EXPECT--
d=15
D=Mon
j=15
l=Monday
N=1
S=th
w=1
z=14
W=03
F=January
m=01
M=Jan
n=1
t=31
L=1
o=2024
X=+2024
x=2024
Y=2024
y=24
a=am
A=AM
B=479
g=10
G=10
h=10
H=10
i=30
s=45
u=000000
v=000
e=UTC
I=0
O=+0000
P=+00:00
p=Z
T=UTC
Z=0
c=2024-01-15T10:30:45+00:00
r=Mon, 15 Jan 2024 10:30:45 +0000
U=1705314645
+05:30|GMT+0530|+05:30|+05:30|+0530|19800|2024-01-15T10:30:45+05:30|Mon, 15 Jan 2024 10:30:45 +0530|1705294845
2024-01-15T10:30:45+05:30
pm PM 12 12
am AM 12 12
2025-W01
2026-W53
0070|70|70|+0070|0070
--CLEAN--
<?php
unset($ts, $t, $d);
