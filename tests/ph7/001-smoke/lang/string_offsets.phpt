--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
String offsets (negative, padding) and subscripting a scalar
--FILE--
<?php
// String offsets, and what happens when a scalar is subscripted. PH7 silently
// produced wrong VALUES here: $s[-1] was NULL, $s[6]="Z" grew "abc" to "abcZ"
// instead of padding, and $x=5; $x[0]=1 replaced the int with an array.
function soTry($label, $fn)
{
    try {
        echo $label, ' => ', var_export($fn(), true), "\n";
    } catch (Throwable $e) {
        echo $label, ' => ', get_class($e), ': ', $e->getMessage(), "\n";
    }
}
set_error_handler(function ($no, $msg) { echo "  [$no] $msg\n"; return true; });

// Reads: negative offsets count back from the end; out of range warns and gives "".
soTry('read 1',       function () { $s = 'abc'; return $s[1]; });
soTry('read -1',      function () { $s = 'abc'; return $s[-1]; });
soTry('read -3',      function () { $s = 'abc'; return $s[-3]; });
soTry('read oob',     function () { $s = 'abc'; return $s[10]; });
soTry('isset oob',    function () { $s = 'abc'; return isset($s[10]); });
soTry('isset -1',     function () { $s = 'abc'; return isset($s[-1]); });
soTry('empty oob',    function () { $s = 'abc'; return empty($s[10]); });

// Writes: negative offsets, space padding, first byte only.
soTry('write 1',      function () { $s = 'abc'; $s[1] = 'Z'; return $s; });
soTry('write -1',     function () { $s = 'abc'; $s[-1] = 'Z'; return $s; });
soTry('write -9',     function () { $s = 'abc'; $s[-9] = 'Z'; return $s; });
soTry('write pad',    function () { $s = 'abc'; $s[6] = 'Z'; return $s; });
soTry('write 2 bytes', function () { $s = 'abc'; $s[1] = 'XY'; return $s; });

// Subscripting a scalar.
soTry('null vivify',  function () { $x = null; $x[0] = 1; return $x; });
soTry('false vivify', function () { $x = false; $x[0] = 1; return $x; });
soTry('true write',   function () { $x = true; $x[0] = 1; return $x; });
soTry('int write',    function () { $x = 5; $x[0] = 1; return $x; });
soTry('int read',     function () { $x = 5; return $x[0]; });
soTry('null read',    function () { $x = null; return $x[0]; });

// foreach over a non-iterable, and implode's array argument.
soTry('foreach int',  function () { $o = []; foreach (5 as $v) { $o[] = $v; } return $o; });
soTry('foreach null', function () { $o = []; foreach (null as $v) { $o[] = $v; } return $o; });
soTry('implode int',  fn() => implode(',', 5));
soTry('implode ok',   fn() => implode(',', [1, 2]));
restore_error_handler();
?>
--EXPECT--
read 1 => 'b'
read -1 => 'c'
read -3 => 'a'
read oob =>   [2] Uninitialized string offset 10
''
isset oob => false
isset -1 => true
empty oob => true
write 1 => 'aZc'
write -1 => 'abZ'
write -9 =>   [2] Illegal string offset -9
'abc'
write pad => 'abc   Z'
write 2 bytes =>   [2] Only the first byte will be assigned to the string offset
'aXc'
null vivify => array (
  0 => 1,
)
false vivify =>   [8192] Automatic conversion of false to array is deprecated
array (
  0 => 1,
)
true write => true write => Error: Cannot use a scalar value as an array
int write => int write => Error: Cannot use a scalar value as an array
int read =>   [2] Trying to access array offset on int
NULL
null read =>   [2] Trying to access array offset on null
NULL
foreach int =>   [2] foreach() argument must be of type array|object, int given
array (
)
foreach null =>   [2] foreach() argument must be of type array|object, null given
array (
)
implode int => implode int => TypeError: implode(): Argument #2 ($array) must be of type ?array, int given
implode ok => '1,2'
--CLEAN--
<?php
