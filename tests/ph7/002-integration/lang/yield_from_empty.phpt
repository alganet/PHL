--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
yield from: empty source yields nothing, numbering continues
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '7.0.0', '<')) echo 'skip Requires PHP 7.0+'; ?>
--FILE--
<?php
function g() {
    yield from [];
    yield "a";
    yield "b";
}
var_export(iterator_to_array(g()));
echo "\n";
?>
--EXPECT--
array (
  0 => 'a',
  1 => 'b',
)
--CLEAN--
<?php
