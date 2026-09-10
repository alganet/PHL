--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Autoloading a base class/interface while compiling the child class
--FILE--
<?php
// Resolving `extends`/`implements` during class compilation triggers the
// autoloader, whose require compiles another file mid-parse. The nested
// compile must not clobber the outer class's parse state.
$dir = sys_get_temp_dir() . '/adc_' . getmypid();
@mkdir($dir);
file_put_contents("$dir/AdcBase.php",
    '<?php class AdcBase { public function b(){ return 7; } }');
file_put_contents("$dir/AdcIface.php",
    '<?php interface AdcIface { public function tag(): string; }');
file_put_contents("$dir/AdcChild.php",
    '<?php class AdcChild extends AdcBase implements AdcIface, Countable {'
  . ' const K = [1, 2, 3];'
  . ' private ?bool $flag = null;'
  . ' public function tag(): string { return "child"; }'
  . ' public function count(): int { return count(self::K); } }');
spl_autoload_register(function ($c) use ($dir) {
    $f = "$dir/$c.php";
    if (is_file($f)) { require $f; }
});

echo class_exists('AdcChild') ? "loaded\n" : "missing\n";
$o = new AdcChild();
echo $o->b(), "\n";
echo $o->tag(), "\n";
echo count($o), "\n";
echo ($o instanceof AdcBase) ? "isbase\n" : "no\n";
echo ($o instanceof AdcIface) ? "isiface\n" : "no\n";

@unlink("$dir/AdcBase.php");
@unlink("$dir/AdcIface.php");
@unlink("$dir/AdcChild.php");
@rmdir($dir);
?>
--EXPECT--
loaded
7
child
3
isbase
isiface
--CLEAN--
<?php
