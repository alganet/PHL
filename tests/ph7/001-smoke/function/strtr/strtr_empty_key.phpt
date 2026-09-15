--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strtr warns about, and ignores, an empty-string key in replace_pairs
--FILE--
<?php
// The empty key is ignored with a warning; 'l' => 'L' still applies.
$result = strtr('hello', array('' => 'x', 'l' => 'L'));
var_dump($result);
?>
--EXPECTF--
%AIgnoring replacement of empty string in %s on line %d
string(5) "heLLo"
--CLEAN--
<?php
unset($result);
