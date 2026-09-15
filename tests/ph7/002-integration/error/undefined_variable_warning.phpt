--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Reading an undefined variable warns (and does not create it); quiet contexts stay silent
--DESCRIPTION--
php raises E_WARNING "Undefined variable $x" for a READ of a missing variable and leaves it
undefined, while isset/empty/??/plain assignment/unset/append are silent. PHL used to yield
NULL silently and, worse, VIVIFY the variable on most reads, so get_defined_vars() disagreed
with php. Read-modify-write forms ($x++, $x .= ...) warn and then seed, as php does. Note
the section markers here avoid a leading "--", which the .phpt parser reads as a section.
--FILE--
<?php
set_error_handler(function ($no, $str) { echo "[$no] $str\n"; return true; });

echo "== warns ==\n";
$r = $uv_read;
$s = "interp=$uv_interp";
$c = 'a' . $uv_concat;
$i = $uv_idx['k'];
$uv_incr++;
$uv_cat .= 'x';
$uv_add += 1;

echo "== quiet ==\n";
$b1 = isset($uv_isset);
$b2 = empty($uv_empty);
$b3 = $uv_coalesce ?? 'default';
$uv_assign = 1;
unset($uv_unset);
$uv_append[] = 5;

echo "== not vivified ==\n";
var_dump(isset($uv_read), isset($uv_interp), isset($uv_isset), isset($uv_coalesce));
var_dump(isset($uv_incr), isset($uv_assign));
?>
--EXPECT--
== warns ==
[2] Undefined variable $uv_read
[2] Undefined variable $uv_interp
[2] Undefined variable $uv_concat
[2] Undefined variable $uv_idx
[2] Trying to access array offset on null
[2] Undefined variable $uv_incr
[2] Undefined variable $uv_cat
[2] Undefined variable $uv_add
== quiet ==
== not vivified ==
bool(false)
bool(false)
bool(false)
bool(false)
bool(true)
bool(true)
