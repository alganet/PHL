--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
dirname edge cases: empty path, roots and trailing separators
--FILE--
<?php
// php strips trailing separators, cuts at the last remaining one, then strips
// trailing separators off the parent as well.
// The root prints as <root> because it carries the PLATFORM separator: php and
// PHL both answer "\\" on Windows and "/" elsewhere.
foreach (["", "/", "//", "///", "a", "a/b", "a//b", "/a", "/a/", "./b", "..", "/.."] as $p) {
    $d = dirname($p);
    echo var_export($p, true), " => ",
        ($d === DIRECTORY_SEPARATOR ? '<root>' : var_export($d, true)), "\n";
}
?>
--EXPECT--
'' => ''
'/' => <root>
'//' => <root>
'///' => <root>
'a' => '.'
'a/b' => 'a'
'a//b' => 'a'
'/a' => <root>
'/a/' => <root>
'./b' => '.'
'..' => '.'
'/..' => <root>
--CLEAN--
<?php
