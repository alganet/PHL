--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A throw out of an included file abandons the statement that included it
--DESCRIPTION--
The eval() half of the same rule lives in 001-smoke/lang. An included file runs
through the nested VmLocalExec that shares the INCLUDING frame, so a throw the
including statement's own try catches runs that catch in place and comes back as a
status — which the include path used to drop, so the assignment and everything
after it in the try body ran on after the catch had already executed. Every form
(include, require, include_once, require_once) takes the same route; a file that
does NOT throw, and the "already included" answer of the _once forms, are
unaffected.
--FILE--
<?php
chdir(__DIR__);
foreach (['a', 'b', 'c', 'd'] as $k) {
    file_put_contents("inc_throw_boom_$k.php", "<?php\nthrow new RuntimeException('from-include-$k');\n");
}
file_put_contents('inc_throw_ok.php', "<?php\nreturn 'value';\n");

try { $r = include 'inc_throw_boom_a.php'; echo "A: not reached r="; var_dump($r); }
catch (\Throwable $e) { echo "A: caught ", $e->getMessage(), "\n"; }

try { $r = require 'inc_throw_boom_b.php'; echo "B: not reached r="; var_dump($r); }
catch (\Throwable $e) { echo "B: caught ", $e->getMessage(), "\n"; }

try { $r = include_once 'inc_throw_boom_c.php'; echo "C: not reached r="; var_dump($r); }
catch (\Throwable $e) { echo "C: caught ", $e->getMessage(), "\n"; }

try { $r = require_once 'inc_throw_boom_d.php'; echo "D: not reached r="; var_dump($r); }
catch (\Throwable $e) { echo "D: caught ", $e->getMessage(), "\n"; }

// already included: _once answers true without running the body again
try { $r = require_once 'inc_throw_boom_d.php'; echo "E: no-throw "; var_dump($r); }
catch (\Throwable $e) { echo "E: caught ", $e->getMessage(), "\n"; }

// a file that returns normally is untouched
try { $r = include 'inc_throw_ok.php'; echo "F: "; var_dump($r); }
catch (\Throwable $e) { echo "F: caught ", $e->getMessage(), "\n"; }

echo "end\n";
?>
--EXPECT--
A: caught from-include-a
B: caught from-include-b
C: caught from-include-c
D: caught from-include-d
E: no-throw bool(true)
F: string(5) "value"
end
--CLEAN--
<?php
foreach (['a', 'b', 'c', 'd'] as $k) { @unlink(__DIR__ . "/inc_throw_boom_$k.php"); }
@unlink(__DIR__ . '/inc_throw_ok.php');
?>
