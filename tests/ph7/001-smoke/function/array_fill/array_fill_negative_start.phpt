--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_fill: a negative start index fills consecutive keys (PHP 8)
--FILE--
<?php
// PHP 8 fills consecutive keys start, start+1, … even when start is negative
// (PHP 7 restarted the remaining keys from 0, giving -5,0,1 instead of -5,-4,-3).
echo json_encode(array_fill(-2, 2, 'z')), "\n";
echo json_encode(array_fill(-5, 3, 0)), "\n";
echo json_encode(array_fill(-3, 5, 'x')), "\n";
echo json_encode(array_fill(5, 3, 0)), "\n";
?>
--EXPECT--
{"-2":"z","-1":"z"}
{"-5":0,"-4":0,"-3":0}
{"-3":"x","-2":"x","-1":"x","0":"x","1":"x"}
{"5":0,"6":0,"7":0}
--CLEAN--
<?php
