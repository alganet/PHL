--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
pack() raises php's format diagnostics, and raises them where php does
--FILE--
<?php
/* Which refusals are ValueErrors and which are warnings, and -- the part a
 * re-implementation gets wrong -- WHEN each one is reached. The whole format is
 * validated before a byte is produced, so an unknown code beats the warning of
 * a code before it; a short hex string is only noticed while the bytes are laid
 * down, which puts it AFTER the unused-argument count. */
function pack_diag(string $label, callable $fn): void {
    $seen = [];
    set_error_handler(function ($no, $msg) use (&$seen) { $seen[] = 'W: ' . $msg; return true; });
    try {
        $out = bin2hex($fn());
    } catch (\Throwable $e) {
        $out = get_class($e) . ': ' . $e->getMessage();
    }
    restore_error_handler();
    foreach ($seen as $line) {
        echo '  ', $line, "\n";
    }
    echo $label, ' -> [', $out, "]\n";
}
echo "## ValueErrors: the format itself\n";
pack_diag('unknown code', fn() => pack('Y', 1));
pack_diag('unknown code after a valid one', fn() => pack('NY', 1, 2));
pack_diag('unknown code beats its own repeater', fn() => pack('Y99999999999', 1));
pack_diag('no value for a fixed field', fn() => pack('c'));
pack_diag('one value short', fn() => pack('c2', 1));
pack_diag('no value for a string field', fn() => pack('H*'));
pack_diag('no value for a starred field', fn() => pack('a*'));

echo "## warnings: the arguments and the cursor\n";
pack_diag('surplus values', fn() => pack('c2', 1, 2, 3));
pack_diag('star on a code that takes no argument', fn() => pack('x*'));
pack_diag('star on X', fn() => pack('X*'));
pack_diag('star on @', fn() => pack('@*'));
pack_diag('X past the start', fn() => pack('X'));
pack_diag('X past the start, twice over', fn() => pack('CX4', 0x41));
pack_diag('hex string shorter than its count', fn() => pack('H4', '12'));
pack_diag('a byte that is not a hex digit', fn() => pack('H1', 'z'));
pack_diag('several bad hex digits', fn() => pack('H*', 'zz'));

echo "## an output that would not fit an int is refused, not attempted\n";
pack_diag('past INT_MAX by one byte', fn() => pack('@2147483647x'));
pack_diag('past INT_MAX by a run', fn() => pack('@2147483000x1000'));
pack_diag('two runs that do not fit together', fn() => pack('x2147483647x'));

echo "## order: the format is validated before anything is written\n";
pack_diag('star warning, then an unknown code', fn() => pack('x*Y'));
pack_diag('unused count, then the short hex string', fn() => pack('H4', '12', 'extra'));

echo "## a value that cannot become a string\n";
pack_diag('object in a fixed field', fn() => pack('a3', new stdClass));
pack_diag('object in a starred field', fn() => pack('a*', new stdClass));
pack_diag('object, with a value left over', fn() => pack('a3', new stdClass, 5));
pack_diag('array in a string field', fn() => pack('a*', [1, 2]));
?>
--EXPECT--
## ValueErrors: the format itself
unknown code -> [ValueError: Type Y: unknown format code]
unknown code after a valid one -> [ValueError: Type Y: unknown format code]
unknown code beats its own repeater -> [ValueError: Type Y: unknown format code]
no value for a fixed field -> [ValueError: Type c: too few arguments]
one value short -> [ValueError: Type c: too few arguments]
no value for a string field -> [ValueError: Type H: not enough arguments]
no value for a starred field -> [ValueError: Type a: not enough arguments]
## warnings: the arguments and the cursor
  W: pack(): 1 arguments unused
surplus values -> [0102]
  W: pack(): Type x: '*' ignored
star on a code that takes no argument -> [00]
  W: pack(): Type X: '*' ignored
  W: pack(): Type X: outside of string
star on X -> []
  W: pack(): Type @: '*' ignored
star on @ -> [00]
  W: pack(): Type X: outside of string
X past the start -> []
  W: pack(): Type X: outside of string
X past the start, twice over -> []
  W: pack(): Type H: not enough characters in string
hex string shorter than its count -> [12]
  W: pack(): Type H: illegal hex digit z
a byte that is not a hex digit -> [00]
  W: pack(): Type H: illegal hex digit z
  W: pack(): Type H: illegal hex digit z
several bad hex digits -> [00]
## an output that would not fit an int is refused, not attempted
past INT_MAX by one byte -> [ValueError: Type x: integer overflow in format string]
past INT_MAX by a run -> [ValueError: Type x: integer overflow in format string]
two runs that do not fit together -> [ValueError: Type x: integer overflow in format string]
## order: the format is validated before anything is written
  W: pack(): Type x: '*' ignored
star warning, then an unknown code -> [ValueError: Type Y: unknown format code]
  W: pack(): 1 arguments unused
  W: pack(): Type H: not enough characters in string
unused count, then the short hex string -> [12]
## a value that cannot become a string
object in a fixed field -> [Error: Object of class stdClass could not be converted to string]
object in a starred field -> [Error: Object of class stdClass could not be converted to string]
  W: pack(): 1 arguments unused
object, with a value left over -> [Error: Object of class stdClass could not be converted to string]
  W: Array to string conversion
array in a string field -> [4172726179]
