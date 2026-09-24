--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (intval('0b0b1', 2) !== 1) echo 'skip strtol() here reads no "0b" prefix of its own (C23 glibc only)'; ?>
--TEST--
intval()'s $base conversion reads a second "0b" prefix, the one C23 strtol() adds after php strips its own
--FILE--
<?php
// php strips intval()'s "0b" itself and hands the rest to the C library's
// strtol(). glibc 2.38+ reads a "0b" prefix of its OWN there (C23); macOS's libc,
// Windows' CRT and older glibc do not, so php's answers below are glibc's.
// PHL reads it on every platform.

// The conversion accepts a binary prefix of its own on top of php's, so exactly
// two are read and a third stops the scan.
var_dump(intval('0b0b1', 2), intval('0b0b11', 0), intval('0b0b0b1', 2), intval('0b0b0b11', 2));
var_dump(intval('0b0B1', 2), intval('0B0b11', 2), intval('-0b0b1', 2), intval('+0b0b11', 2));
var_dump(intval('0b0b', 2), intval('0b0b0', 2), intval('0b0b2', 2), intval('0b0b-1', 2));
var_dump(intval('0b0b1x', 2), intval('0b-0b1', 2), intval('0b 0b1', 2), intval('0b0x1', 2));
// A base that narrows to 0 past the full-width "0b" strip test gets no strip at
// all, so the conversion's own prefix is the only one read.
var_dump(intval('0b101', 4294967296));

// The "0b" STRIP reads the base at full width, one step before that cast, so a
// base of 2^32+2 converts in base 2 without stripping -- leaving only the
// prefix the conversion itself reads, which needs a binary digit behind it.
var_dump(intval('0b101', 4294967298), intval('0b-1', 4294967298), intval('0b-1', 2));
var_dump(intval('0b0b1', 4294967298), intval('0b 1', 4294967296), intval('0b 1', 0));
var_dump(intval('0b101', PHP_INT_MIN), intval('-0b1', 4294967296), intval('+0b11', 8589934592));
?>
--EXPECT--
int(1)
int(3)
int(0)
int(0)
int(1)
int(3)
int(-1)
int(3)
int(0)
int(0)
int(0)
int(0)
int(1)
int(-1)
int(1)
int(0)
int(5)
int(5)
int(0)
int(-1)
int(0)
int(0)
int(1)
int(5)
int(-1)
int(3)
