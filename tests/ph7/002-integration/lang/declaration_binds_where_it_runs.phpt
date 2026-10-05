--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A declaration whose dependency is not declared yet asks the autoloader when its statement runs, never while its unit compiles
--FILE--
<?php
spl_autoload_register(function ($c) {
    echo "autoload $c\n";
    if ($c === 'Missing') return;
    eval(str_starts_with($c, 'I') ? "interface $c {}" : (str_starts_with($c, 'T') ? "trait $c {}" : "class $c {}"));
});

// the statements above a declaration run before its parent is asked for
eval('echo "before\n"; class U extends A {} echo "after\n";');
var_dump(get_parent_class('U'));

// a unit that fails to parse runs nothing, so it asks the autoloader nothing
try { eval('class V extends B {} interface IV extends IB {} $x = ;'); }
catch (ParseError $e) { echo $e->getMessage(), "\n"; }
var_dump(class_exists('V', false), class_exists('B', false));

// a parent declared further down the same unit is never autoloaded
eval('class W extends Later {} class Later {} echo "W ok\n";');
var_dump(get_parent_class('W'));

// parent, then traits, then interfaces, each asked when the statement runs
eval('echo "x\n"; class X extends PX implements IX { use TX; }');
var_dump(class_implements('X'), class_uses('X'));

// include takes the same path
$file = tempnam(sys_get_temp_dir(), 'phl');
file_put_contents($file, '<?php echo "inc\n"; class Y extends PY {} $x = ;');
try { include $file; } catch (ParseError $e) { echo $e->getMessage(), "\n"; }
file_put_contents($file, '<?php echo "inc\n"; class Z extends PZ {} echo "inc done\n";');
include $file;
unlink($file);
var_dump(class_exists('Y', false), class_exists('PY', false), get_parent_class('Z'));

// a parent nobody can provide fails at the statement, after the lines above it
try { eval('echo "reached\n"; class M extends Missing {}'); }
catch (Error $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
?>
--EXPECT--
before
autoload A
after
string(1) "A"
syntax error, unexpected token ";"
bool(false)
bool(false)
W ok
string(5) "Later"
x
autoload PX
autoload TX
autoload IX
array(1) {
  ["IX"]=>
  string(2) "IX"
}
array(1) {
  ["TX"]=>
  string(2) "TX"
}
syntax error, unexpected token ";"
inc
autoload PZ
inc done
bool(false)
bool(false)
string(2) "PZ"
reached
autoload Missing
Error: Class "Missing" not found
