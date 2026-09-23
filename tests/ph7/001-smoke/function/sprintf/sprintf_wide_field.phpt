--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A sprintf field is not capped at the conversion buffer, and a bare '.' is no precision
--FILE--
<?php
// The padding is emitted in chunks, so nothing about the field width has to fit
// in the 1024-byte conversion buffer; only the zero fill did, and it no longer
// goes there -- a '0'-padded number hands its sign to the output instead.
foreach (['%2000d', '%02000d', '%+02000d', "%'x2000d", '%-2000d|', '%-02000d|',
          '%02000.3f', '%2000s', '%02000s', '%02000x', '%02000b'] as $sprintfWideF) {
    foreach ([5, -5, -3.25] as $sprintfWideV) {
        $sprintfWideR = sprintf($sprintfWideF, $sprintfWideV);
        echo $sprintfWideF, ' ', $sprintfWideV, ' len=', strlen($sprintfWideR),
            ' head=', substr($sprintfWideR, 0, 4),
            ' tail=', substr($sprintfWideR, -4), "\n";
    }
}
// php only has a precision where a DIGIT follows the '.'; a bare one is a zero
// nothing consults.
var_dump(sprintf('%.s', 'abc'), sprintf('%5.s', 'abc'), sprintf('%.x', 42),
    sprintf('%.d', 42), sprintf('%.f', 3.5));
var_dump(sprintf('%.0s', 'abc'), sprintf('%.1s', 'abc'), sprintf('%.0x', 42));
--EXPECT--
%2000d 5 len=2000 head=     tail=   5
%2000d -5 len=2000 head=     tail=  -5
%2000d -3.25 len=2000 head=     tail=  -3
%02000d 5 len=2000 head=0000 tail=0005
%02000d -5 len=2000 head=-000 tail=0005
%02000d -3.25 len=2000 head=-000 tail=0003
%+02000d 5 len=2000 head=+000 tail=0005
%+02000d -5 len=2000 head=-000 tail=0005
%+02000d -3.25 len=2000 head=-000 tail=0003
%'x2000d 5 len=2000 head=xxxx tail=xxx5
%'x2000d -5 len=2000 head=xxxx tail=xx-5
%'x2000d -3.25 len=2000 head=xxxx tail=xx-3
%-2000d| 5 len=2001 head=5    tail=   |
%-2000d| -5 len=2001 head=-5   tail=   |
%-2000d| -3.25 len=2001 head=-3   tail=   |
%-02000d| 5 len=2001 head=5    tail=   |
%-02000d| -5 len=2001 head=-5   tail=   |
%-02000d| -3.25 len=2001 head=-3   tail=   |
%02000.3f 5 len=2000 head=0000 tail=.000
%02000.3f -5 len=2000 head=-000 tail=.000
%02000.3f -3.25 len=2000 head=-000 tail=.250
%2000s 5 len=2000 head=     tail=   5
%2000s -5 len=2000 head=     tail=  -5
%2000s -3.25 len=2000 head=     tail=3.25
%02000s 5 len=2000 head=0000 tail=0005
%02000s -5 len=2000 head=0000 tail=00-5
%02000s -3.25 len=2000 head=0000 tail=3.25
%02000x 5 len=2000 head=0000 tail=0005
%02000x -5 len=2000 head=0000 tail=fffb
%02000x -3.25 len=2000 head=0000 tail=fffd
%02000b 5 len=2000 head=0000 tail=0101
%02000b -5 len=2000 head=0000 tail=1011
%02000b -3.25 len=2000 head=0000 tail=1101
string(3) "abc"
string(5) "  abc"
string(2) "2a"
string(2) "42"
string(1) "4"
string(0) ""
string(1) "a"
string(0) ""
--CLEAN--
<?php
