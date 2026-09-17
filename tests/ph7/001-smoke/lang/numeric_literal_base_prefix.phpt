--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A base prefix is only read after a lone 0 followed by at least one valid digit; everything else is 0 plus an identifier
--DESCRIPTION--
The malformed forms are parse errors and so cannot live in this file; they are
covered by 002-integration/lang/invalid_hex_literal.phpt and the probe matrix.
The exponent case is cast to int because var_dump renders a whole float
in exponential notation in PHL (a separate recorded rendering gap);
what matters here is that the literal still tokenizes to the right value. What is asserted here is that the VALID forms still tokenize, which is
the regression risk in gating the prefix.
--FILE--
<?php
var_dump(0xA, 0XFF, 0x1_F);      // hex
var_dump(0b11, 0B1_1);           // binary
var_dump(0o17, 0O17);            // explicit octal (php 8.1)
var_dump(017, 007);              // legacy octal
var_dump(0, 00, 1_000);          // plain
var_dump((int)1.5e3, 2.5e-3);    // exponent form still parses
?>
--EXPECT--
int(10)
int(255)
int(31)
int(3)
int(3)
int(15)
int(15)
int(15)
int(7)
int(0)
int(0)
int(1000)
int(1500)
float(0.0025)
--CLEAN--
<?php
