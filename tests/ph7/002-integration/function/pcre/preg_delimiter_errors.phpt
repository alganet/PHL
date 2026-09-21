--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg delimiter warnings name the delimiter char and flag paired/bad delimiters (php-exact)
--INI--
display_errors=1
log_errors=0
--FILE--
<?php
echo "START\n";
preg_match('/x', 'x');            // unterminated, unpaired
preg_match('(x', 'x');            // unterminated, paired ()
preg_match('[x', 'x');            // unterminated, paired []
preg_match('{x', 'x');            // unterminated, paired {}
preg_match('<x', 'x');            // unterminated, paired <>
preg_match('#x', 'x');            // unterminated, unpaired (# delimiter)
preg_match('1x1', 'x');           // alphanumeric delimiter
preg_split('/a', 'x');            // shares the same delimiter parser
preg_replace('(a', 'b', 'x');     // paired, via preg_replace
echo "END\n";
?>
--EXPECTF--
START

Warning: preg_match(): No ending delimiter '/' found in %s on line %d

Warning: preg_match(): No ending matching delimiter ')' found in %s on line %d

Warning: preg_match(): No ending matching delimiter ']' found in %s on line %d

Warning: preg_match(): No ending matching delimiter '}' found in %s on line %d

Warning: preg_match(): No ending matching delimiter '>' found in %s on line %d

Warning: preg_match(): No ending delimiter '#' found in %s on line %d

Warning: preg_match(): Delimiter must not be alphanumeric, backslash, or NUL byte in %s on line %d

Warning: preg_split(): No ending delimiter '/' found in %s on line %d

Warning: preg_replace(): No ending matching delimiter ')' found in %s on line %d
END
