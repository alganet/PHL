--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A throw out of eval()/include abandons the statement that contained it
--FILE--
<?php
// the catch is in the SAME frame as the eval: php runs the catch and abandons
// the rest of the statement — the assignment and everything after it in the try
try { $r = eval('throw new \RuntimeException("e1");'); echo "A: not reached r="; var_dump($r); }
catch (\Throwable $e) { echo "A: caught ", $e->getMessage(), "\n"; }

// a throw the engine raises inside the chunk, not a written `throw`
try { $r = eval('return 1 + [];'); echo "B: not reached r="; var_dump($r); }
catch (\Throwable $e) { echo "B: caught ", $e->getMessage(), "\n"; }

// mid-expression: the whole expression goes, not just the eval
try { $x = eval('return 5;') + eval('throw new \Exception("mid");'); echo "C: not reached x=$x"; }
catch (\Throwable $e) { echo "C: caught ", $e->getMessage(), "\n"; }

// a finally still runs, and the catch still sees it
try {
    try { eval('throw new \LogicException("inner");'); echo "D: not reached"; }
    finally { echo "D: finally "; }
} catch (\Throwable $e) { echo "D: caught ", $e->getMessage(), "\n"; }

// an eval that does NOT throw is unaffected, in a loop and out of it
foreach ([1, 2] as $i) {
    try { $v = eval("return \$i * 2;"); echo "E$i=$v "; } catch (\Throwable $e) { echo "E?"; }
}
echo "\n";

// a compile failure in the chunk is php's catchable ParseError, and the
// statement is abandoned the same way
try { $r = eval('syntax ((('); echo "F: not reached r="; var_dump($r); }
catch (\ParseError $e) { echo "F: caught ParseError\n"; }

echo "end\n";
?>
--EXPECT--
A: caught e1
B: caught Unsupported operand types: int + array
C: caught mid
D: finally D: caught inner
E1=2 E2=4 
F: caught ParseError
end
--CLEAN--
<?php
