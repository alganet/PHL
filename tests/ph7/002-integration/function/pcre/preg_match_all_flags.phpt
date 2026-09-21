--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg_match_all() validates $flags: PREG_* mask + PATTERN_ORDER/SET_ORDER are mutually exclusive
--FILE--
<?php
function pmaf($l, $fn) { try { $m=[]; echo "$l => ", var_export($fn($m), true), "\n"; } catch (Throwable $e) { echo "$l => ", get_class($e), ": ", $e->getMessage(), "\n"; } }
// valid: exactly one order flag, optionally with offset/unmatched
pmaf('flags 0',   fn(&$m) => preg_match_all('/x/', 'x', $m, 0));
pmaf('flags 1',   fn(&$m) => preg_match_all('/x/', 'x', $m, PREG_PATTERN_ORDER));
pmaf('flags 2',   fn(&$m) => preg_match_all('/x/', 'x', $m, PREG_SET_ORDER));
pmaf('flags 257', fn(&$m) => preg_match_all('/x/', 'x', $m, PREG_PATTERN_ORDER|PREG_OFFSET_CAPTURE));
pmaf('flags 770', fn(&$m) => preg_match_all('/x/', 'x', $m, PREG_SET_ORDER|PREG_OFFSET_CAPTURE|PREG_UNMATCHED_AS_NULL));
pmaf('flags 1024', fn(&$m) => preg_match_all('/x/', 'x', $m, 1024)); // high bits ignored by php 8.5
// invalid: out-of-mask, or PATTERN_ORDER+SET_ORDER together (mutually exclusive)
pmaf('flags 3',   fn(&$m) => preg_match_all('/x/', 'x', $m, PREG_PATTERN_ORDER|PREG_SET_ORDER));
pmaf('flags 259', fn(&$m) => preg_match_all('/x/', 'x', $m, PREG_PATTERN_ORDER|PREG_SET_ORDER|PREG_OFFSET_CAPTURE));
pmaf('flags 4',   fn(&$m) => preg_match_all('/x/', 'x', $m, 4));
pmaf('flags 999', fn(&$m) => preg_match_all('/x/', 'x', $m, 999));
?>
--EXPECT--
flags 0 => 1
flags 1 => 1
flags 2 => 1
flags 257 => 1
flags 770 => 1
flags 1024 => 1
flags 3 => flags 3 => ValueError: preg_match_all(): Argument #4 ($flags) must be a PREG_* constant
flags 259 => flags 259 => ValueError: preg_match_all(): Argument #4 ($flags) must be a PREG_* constant
flags 4 => flags 4 => ValueError: preg_match_all(): Argument #4 ($flags) must be a PREG_* constant
flags 999 => flags 999 => ValueError: preg_match_all(): Argument #4 ($flags) must be a PREG_* constant
