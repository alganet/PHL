--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
eval() compiles as if it began after `<?php`, so a `?>` inside it leaves php mode
--FILE--
<?php
/* `eval('?>' . file_get_contents($f))` is the ordinary way to run a php FILE's
 * bytes -- composer reloads its own vendor/composer/installed.php exactly that
 * way -- and the whole chunk used to be handed to the compiler as one php token,
 * so the `?>` was a syntax error. */
function evalShow($code)
{
	echo '[', str_replace("\n", '\n', $code), '] => ';
	try { var_export(eval($code)); }
	catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(); }
	echo "\n";
}
evalShow('return 1;');
evalShow('?><?php return 2;');
evalShow('?>text<?php return 3;');
evalShow('?>only text');
evalShow('?>');
evalShow('return 4; ?>');
evalShow('return 5; ?>tail');
evalShow("?><?php\nreturn 6;");
evalShow('?><?php return array("a" => 1, "b" => array(2));');
evalShow('echo "e"; return 7;');
evalShow('?><?php function evalModeFn() { return 8; } return evalModeFn();');
evalShow('?><?php class EvalModeC { const K = 9; } return EvalModeC::K;');
evalShow("\n\nreturn __LINE__;");
evalShow('$evalModeV = 10; return $evalModeV;');
evalShow('?>a<?php $x = 11; ?>b<?php return $x;');
/* An empty chunk is php's false, and a whitespace-only one compiles to null. */
evalShow('');
evalShow('   ');
?>
--EXPECT--
[return 1;] => 1
[?><?php return 2;] => 2
[?>text<?php return 3;] => text3
[?>only text] => only textNULL
[?>] => NULL
[return 4; ?>] => 4
[return 5; ?>tail] => 5
[?><?php\nreturn 6;] => 6
[?><?php return array("a" => 1, "b" => array(2));] => array (
  'a' => 1,
  'b' => 
  array (
    0 => 2,
  ),
)
[echo "e"; return 7;] => e7
[?><?php function evalModeFn() { return 8; } return evalModeFn();] => 8
[?><?php class EvalModeC { const K = 9; } return EvalModeC::K;] => 9
[\n\nreturn __LINE__;] => 3
[$evalModeV = 10; return $evalModeV;] => 10
[?>a<?php $x = 11; ?>b<?php return $x;] => ab11
[] => false
[   ] => NULL
