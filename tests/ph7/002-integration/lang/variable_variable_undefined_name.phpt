--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A variable-variable ($$n / ${$expr}) warns on an undefined NAME, in write context too
--DESCRIPTION--
The NAME operand of a variable-variable is a pure read: php warns `Undefined variable $n` when
it is undefined. PHL compiled the name-load in the SAME context as the whole `$$n`, so in a
write (`$$n = 5`, `$$n =& $x`) the name inherited create-mode and silently invented $n instead
of warning; the `${$expr}` curly form compiled its name expression non-read-only for the same
reason. Now every load except the final dereference is a read-only name resolution. isset()/
empty() suppress the name warning (as php does); `??` does NOT — it warns the name and quiets
only the target read (php's EXPR_FLAG_QUIET_VAR is scoped to the coalesced value, not the name).
--FILE--
<?php
set_error_handler(function ($no, $str) { echo "  [$no] $str\n"; return true; });

echo "== write context warns on an undefined name ==\n";
(function () { $$n = 5; ${$u} = 6; })();

echo "== isset()/empty() suppress the name warning ==\n";
var_dump(isset($$a), empty(${$b}));

echo "== coalesce warns the NAME, quiets the target ==\n";
var_dump($$c ?? "def");

echo "== defined name resolves and writes normally ==\n";
$k = "v"; $$k = 41; var_dump($v);
$p = "q"; ${$p} = 42; var_dump($q);

echo "== chained var-var ==\n";
$a = "b"; $b = "c"; $c = 9; var_dump($$$a);
?>
--EXPECT--
== write context warns on an undefined name ==
  [2] Undefined variable $n
  [2] Undefined variable $u
== isset()/empty() suppress the name warning ==
bool(false)
bool(true)
== coalesce warns the NAME, quiets the target ==
  [2] Undefined variable $c
string(3) "def"
== defined name resolves and writes normally ==
int(41)
int(42)
== chained var-var ==
int(9)
--CLEAN--
<?php
unset($k, $v, $p, $q, $a, $b, $c);
