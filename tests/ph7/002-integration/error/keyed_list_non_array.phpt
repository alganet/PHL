--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Keyed list destructuring from a non-array source yields NULL + warns (not char-index)
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '8.5.0', '<')) echo 'skip'; ?>
--FILE--
<?php
["a" => $a] = "string";   // string source: warn, $a NULL (NOT char-indexed to "s")
["b" => $i] = 42;         // int source: warn, $i NULL
["c" => $n] = null;       // null source: silent, $n NULL
echo isset($a) ? "a-set" : "a-unset", "\n";
echo isset($i) ? "i-set" : "i-unset", "\n";
echo isset($n) ? "n-set" : "n-unset", "\n";
?>
--EXPECTF--
%s Warning:  Cannot use string as array%A Warning:  Cannot use int as array%Aa-unset%Ai-unset%An-unset
--CLEAN--
<?php
unset($a, $i, $n);
