--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg_match auto-vivifies $matches via the absolute \preg_match() form inside a namespace
--FILE--
<?php
namespace App;
// The fully-qualified \preg_match resolves to the global builtin and must
// still auto-vivify the undefined $m.
\preg_match('/(\d+)/', 'a12', $m);
echo $m[0] . "\n";
echo $m[1] . "\n";
?>
--EXPECT--
12
12
--CLEAN--
<?php
