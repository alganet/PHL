--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed parameter: an unfolded default under a strict caller
--FILE--
<?php
declare(strict_types=1);
// A strict caller holds an unfolded default to the type strictly; an int
// still widens to float.
const PDCTS_N = "5";
function pdcts_num(int $x = PDCTS_N) { return $x; }
function pdcts_wide(float $x = PHP_INT_SIZE) { return $x; }
try { var_dump(pdcts_num()); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
var_dump(pdcts_wide());
--EXPECTF--
pdcts_num(): Argument #1 ($x) must be of type int, string given, called in %s on line 8
float(8)
