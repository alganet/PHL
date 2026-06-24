--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
yield from: aliased trait generator method
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '7.0.0', '<')) echo 'skip Requires PHP 7.0+'; ?>
--FILE--
<?php
trait Source2 { function raw() { yield from ["a", "b"]; } }
class Repo2 { use Source2 { raw as fetch; } }
$r = new Repo2;
echo implode(",", iterator_to_array($r->fetch(), false)), "\n";
?>
--EXPECT--
a,b
--CLEAN--
<?php
