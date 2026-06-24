--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
yield from: a trait method whose only generator marker is yield from
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '7.0.0', '<')) echo 'skip Requires PHP 7.0+'; ?>
--FILE--
<?php
trait Source { function items() { yield from [1, 2, 3]; } }
class Repo { use Source; }
$r = new Repo;
echo implode(",", iterator_to_array($r->items(), false)), "\n";
?>
--EXPECT--
1,2,3
--CLEAN--
<?php
