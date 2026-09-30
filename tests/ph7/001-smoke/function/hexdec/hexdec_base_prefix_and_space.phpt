--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
hexdec/bindec/octdec/base_convert accept php's three silent spellings: surrounding whitespace, and the base's own literal prefix
--FILE--
<?php
// php accepts these three spellings with no diagnostic at all: leading/trailing
// whitespace, and the base's own literal prefix.
foreach (['0xff', '0XFF', '0x', 'ff', '  1f  ', "\t1f\n", '  0x1f  '] as $hbp_s) {
    printf("hexdec(%s) = %s\n", var_export($hbp_s, true), var_export(hexdec($hbp_s), true));
}
foreach (['0b1101', '0B1101', '0b', '1101', ' 1101 '] as $hbp_s) {
    printf("bindec(%s) = %s\n", var_export($hbp_s, true), var_export(bindec($hbp_s), true));
}
foreach (['0o17', '0O17', '0o', '17', ' 17 '] as $hbp_s) {
    printf("octdec(%s) = %s\n", var_export($hbp_s, true), var_export(octdec($hbp_s), true));
}
// base_convert shares the front end, and the prefix is stripped only for the
// base that owns it: in base 36 the 'x' of "0x1f" is the digit 33.
foreach ([['0x1f', 16], ['0b11', 2], ['0o17', 8], ['0x1f', 36], ['0b11', 36], [' 12 ', 10]] as [$hbp_n, $hbp_b]) {
    printf("base_convert(%s, %d, 10) = %s\n", var_export($hbp_n, true), $hbp_b,
        var_export(base_convert($hbp_n, $hbp_b, 10), true));
}
// The prefix is not a value: "0xffffffffffffffff" still overflows to a float.
var_dump(hexdec('0xffffffffffffffff'));
?>
--EXPECT--
hexdec('0xff') = 255
hexdec('0XFF') = 255
hexdec('0x') = 0
hexdec('ff') = 255
hexdec('  1f  ') = 31
hexdec('	1f
') = 31
hexdec('  0x1f  ') = 31
bindec('0b1101') = 13
bindec('0B1101') = 13
bindec('0b') = 0
bindec('1101') = 13
bindec(' 1101 ') = 13
octdec('0o17') = 15
octdec('0O17') = 15
octdec('0o') = 0
octdec('17') = 15
octdec(' 17 ') = 15
base_convert('0x1f', 16, 10) = '31'
base_convert('0b11', 2, 10) = '3'
base_convert('0o17', 8, 10) = '15'
base_convert('0x1f', 36, 10) = '42819'
base_convert('0b11', 36, 10) = '14293'
base_convert(' 12 ', 10, 10) = '12'
float(1.8446744073709552E+19)
