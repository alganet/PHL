--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
memory_limit stops the script with php's fatal instead of the OS killing the process
--DESCRIPTION--
php enforces memory_limit as a total live-byte ceiling and kills the script with
`Allowed memory size of N bytes exhausted`. PHL used to STORE the directive and
enforce nothing, so a runaway allocation had no ceiling below the kernel's and the
OOM killer took the whole process -- on a shared machine, along with whatever else
it picked. The ceiling is armed from the directive here and the refusal is raised
between two instructions, where the operand stack is consistent.

The limit is set relative to what the script has ALREADY used, so the test does not
depend on the engine's start-up footprint.

php prints a `Stack trace:` block under this fatal and PHL does not, which is why
this lives here rather than in the cross-engine corpus.
--FILE--
<?php
ini_set('memory_limit', (memory_get_usage() + 2097152) . '');
$a = [];
for ($i = 0; $i < 1000000; $i++) { $a[] = str_repeat('x', 1000); }
echo "UNREACHABLE\n";
?>
--EXPECTF--
PHP Fatal error:  Allowed memory size of %d bytes exhausted (tried to allocate %d bytes) in %s on line %d
--CLEAN--
<?php
