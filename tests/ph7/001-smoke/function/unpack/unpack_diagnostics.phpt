--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
unpack() answers FALSE with a warning where php does, and throws where php throws
--FILE--
<?php
/* unpack()'s failures split three ways, and which is which is php's: an unknown
 * code and an offset outside the input are ValueErrors raised before anything is
 * read, input that runs out mid-field is an E_WARNING and FALSE (the whole array
 * is discarded, not returned half-filled), and a `*` run simply STOPS when the
 * input does. */
function unpack_diag(string $label, callable $fn): void {
    $seen = [];
    set_error_handler(function ($no, $msg) use (&$seen) { $seen[] = 'W: ' . $msg; return true; });
    try {
        $r = $fn();
        $out = json_encode($r === false ? false
            : array_map(fn($v) => is_string($v) ? bin2hex($v) : $v, $r));
    } catch (\Throwable $e) {
        $out = get_class($e) . ': ' . $e->getMessage();
    }
    restore_error_handler();
    foreach ($seen as $line) {
        echo '  ', $line, "\n";
    }
    echo $label, ' -> ', $out, "\n";
}
echo "## ValueErrors\n";
unpack_diag('unknown code', fn() => unpack('Y', 'ab'));
unpack_diag('unknown code after a valid one', fn() => unpack('Ca/Yb', 'ab'));
unpack_diag('offset past the end', fn() => unpack('C', 'abc', 5));
unpack_diag('negative offset', fn() => unpack('C', 'abc', -1));
unpack_diag('offset at the end is allowed', fn() => unpack('C*', 'abc', 3));

echo "## input that runs out\n";
unpack_diag('four bytes wanted, two there', fn() => unpack('N', 'ab'));
unpack_diag('one byte wanted, none there', fn() => unpack('A2', 'ab', 2));
unpack_diag('a five-byte field, two there', fn() => unpack('a5s', 'ab'));
unpack_diag('the second entry runs out', fn() => unpack('Ca/Nb', "\x01\x02"));
unpack_diag('a repeated entry runs out', fn() => unpack('C4n', 'ab'));
unpack_diag('a star run just stops', fn() => unpack('N*', 'abcdef'));
unpack_diag('a star run with nothing left', fn() => unpack('N*', 'ab'));

echo "## the cursor codes at the edges\n";
unpack_diag('X at the start', fn() => unpack('X', 'abc'));
unpack_diag('X past the start', fn() => unpack('CX4C', 'abc'));
unpack_diag('star on X', fn() => unpack('X*', 'abc'));
unpack_diag('@ past the end', fn() => unpack('@5C', "\x05\x06"));
unpack_diag('@ exactly at the end', fn() => unpack('@3C', 'abc'));

echo "## a repeater no int can hold\n";
unpack_diag('overflowing repeater', fn() => unpack('N4294967296x', 'abcd'));
unpack_diag('overflowing hex repeater', fn() => unpack('H99999999999', 'abcd'));

echo "## an empty format reads nothing\n";
unpack_diag('empty format', fn() => unpack('', 'abc'));
unpack_diag('zero repetitions', fn() => unpack('C0a', "\x05"));
?>
--EXPECT--
## ValueErrors
unknown code -> ValueError: Invalid format type Y
unknown code after a valid one -> ValueError: Invalid format type Y
offset past the end -> ValueError: unpack(): Argument #3 ($offset) must be contained in argument #2 ($data)
negative offset -> ValueError: unpack(): Argument #3 ($offset) must be contained in argument #2 ($data)
offset at the end is allowed -> []
## input that runs out
  W: unpack(): Type N: not enough input values, need 4 values but only 2 were provided
four bytes wanted, two there -> false
  W: unpack(): Type A: not enough input values, need 2 values but only 0 were provided
one byte wanted, none there -> false
  W: unpack(): Type a: not enough input values, need 5 values but only 2 were provided
a five-byte field, two there -> false
  W: unpack(): Type N: not enough input values, need 4 values but only 1 was provided
the second entry runs out -> false
  W: unpack(): Type C: not enough input values, need 1 values but only 0 were provided
a repeated entry runs out -> false
a star run just stops -> {"1":1633837924}
a star run with nothing left -> []
## the cursor codes at the edges
X at the start -> []
X past the start -> {"X4C":97}
  W: unpack(): Type X: '*' ignored
star on X -> []
  W: unpack(): Type @: outside of string
@ past the end -> []
@ exactly at the end -> []
## a repeater no int can hold
  W: unpack(): Type N: integer overflow
overflowing repeater -> false
  W: unpack(): Type H: integer overflow
overflowing hex repeater -> false
## an empty format reads nothing
empty format -> []
zero repetitions -> []
