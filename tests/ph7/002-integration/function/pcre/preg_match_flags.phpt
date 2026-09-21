--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg_match() validates $flags: only PREG_OFFSET_CAPTURE/PREG_UNMATCHED_AS_NULL, else ValueError
--FILE--
<?php
function pmf($l, $fn) { try { $m=[]; echo "$l => ", var_export($fn($m), true), "\n"; } catch (Throwable $e) { echo "$l => ", get_class($e), ": ", $e->getMessage(), "\n"; } }
// valid flags: no throw
pmf('flags 0',   fn(&$m) => preg_match('/x/', 'x', $m, 0));
pmf('flags 256', fn(&$m) => preg_match('/x/', 'x', $m, PREG_OFFSET_CAPTURE));
pmf('flags 512', fn(&$m) => preg_match('/z/', 'x', $m, PREG_UNMATCHED_AS_NULL));
pmf('flags 768', fn(&$m) => preg_match('/x/', 'x', $m, PREG_OFFSET_CAPTURE|PREG_UNMATCHED_AS_NULL));
pmf('flags 1024', fn(&$m) => preg_match('/x/', 'x', $m, 1024)); // high bits ignored by php 8.5
// invalid flags: ValueError (PATTERN_ORDER/SET_ORDER belong to preg_match_all only)
pmf('flags 1',   fn(&$m) => preg_match('/x/', 'x', $m, PREG_PATTERN_ORDER));
pmf('flags 2',   fn(&$m) => preg_match('/x/', 'x', $m, PREG_SET_ORDER));
pmf('flags 8',   fn(&$m) => preg_match('/x/', 'x', $m, 8));
pmf('flags 257', fn(&$m) => preg_match('/x/', 'x', $m, PREG_OFFSET_CAPTURE|PREG_PATTERN_ORDER));
pmf('flags 999', fn(&$m) => preg_match('/x/', 'x', $m, 999));
?>
--EXPECT--
flags 0 => 1
flags 256 => 1
flags 512 => 0
flags 768 => 1
flags 1024 => 1
flags 1 => flags 1 => ValueError: preg_match(): Argument #4 ($flags) must be a PREG_* constant
flags 2 => flags 2 => ValueError: preg_match(): Argument #4 ($flags) must be a PREG_* constant
flags 8 => flags 8 => ValueError: preg_match(): Argument #4 ($flags) must be a PREG_* constant
flags 257 => flags 257 => ValueError: preg_match(): Argument #4 ($flags) must be a PREG_* constant
flags 999 => flags 999 => ValueError: preg_match(): Argument #4 ($flags) must be a PREG_* constant
