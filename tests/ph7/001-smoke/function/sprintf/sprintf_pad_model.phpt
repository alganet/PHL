--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
sprintf has ONE pad character per specifier, and the last pad flag wins
--FILE--
<?php
// php keeps a single `padding` byte: ' ' selects a space, '0' a zero and "'<c>"
// any byte -- so the LAST of them wins, and ' ' is a pad selector rather than
// C's space-for-a-positive-sign.
$sprintfPadModel = [
    "% d",     // no sign prefix: php has no blank-sign flag
    "% 5d", "% -5d|", "% 05d", "%0 5d", "%0'x5d", "%'x05d", "% '_8d", "%'_ 8d",
    "%'05d", "%+'05d", "%+05d",
    "%-05d|", "%-+05d|", "%-08u|",
    // ...but only %d and %u refuse to right-pad with zeros; the other radices
    // and the float conversions do it.
    "%-08x|", "%-08b|", "%-08o|", "%-08.2f|", "%-05s|",
    "%'08.2f|", "%0 8.2f|",
];
foreach ($sprintfPadModel as $sprintfPadFmt) {
    printf("%-10s 5=[%s] -5=[%s]\n", $sprintfPadFmt,
        sprintf($sprintfPadFmt, 5), sprintf($sprintfPadFmt, -5));
}
// A precision never reaches %d/%u, and on the other radices php's append2n
// truncates the digits to nothing while still honouring the width.
var_dump(sprintf('%.5d', 42), sprintf('%.5u', 42), sprintf('%8.5d', 42));
var_dump(sprintf('%.5x', 42), sprintf('%.0b', 42), sprintf('%5.1x', 42));
// %c carries no field at all.
var_dump(sprintf('%5c', 65), sprintf('%-5c|', 65));
// A conversion that substitutes nothing leaves the empty string, not NULL.
var_dump(sprintf('%s', ''), sprintf('%s', false), sprintf('%s%s', '', null));
--EXPECT--
% d        5=[5] -5=[-5]
% 5d       5=[    5] -5=[   -5]
% -5d|     5=[5    |] -5=[-5   |]
% 05d      5=[00005] -5=[-0005]
%0 5d      5=[    5] -5=[   -5]
%0'x5d     5=[xxxx5] -5=[xxx-5]
%'x05d     5=[00005] -5=[-0005]
% '_8d     5=[_______5] -5=[______-5]
%'_ 8d     5=[       5] -5=[      -5]
%'05d      5=[00005] -5=[-0005]
%+'05d     5=[+0005] -5=[-0005]
%+05d      5=[+0005] -5=[-0005]
%-05d|     5=[5    |] -5=[-5   |]
%-+05d|    5=[+5   |] -5=[-5   |]
%-08u|     5=[5       |] -5=[18446744073709551611|]
%-08x|     5=[50000000|] -5=[fffffffffffffffb|]
%-08b|     5=[10100000|] -5=[1111111111111111111111111111111111111111111111111111111111111011|]
%-08o|     5=[50000000|] -5=[1777777777777777777773|]
%-08.2f|   5=[5.000000|] -5=[-5.00000|]
%-05s|     5=[50000|] -5=[-5000|]
%'08.2f|   5=[00005.00|] -5=[-0005.00|]
%0 8.2f|   5=[    5.00|] -5=[   -5.00|]
string(2) "42"
string(2) "42"
string(8) "      42"
string(0) ""
string(0) ""
string(5) "     "
string(1) "A"
string(2) "A|"
string(0) ""
string(0) ""
string(0) ""
--CLEAN--
<?php
