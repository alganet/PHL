--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A diagnostic a unit spl_autoload() loaded raises calls the error handler from that unit
--DESCRIPTION--
spl_autoload() compiles and runs the file it finds in its caller's activation. A
warning the top level of that file raises is the file's own, and php calls the error
handler from it: the handler's frame carries the file and line, and a handler that
wants more arguments than it is given names the file they were "passed in". This
engine took spl_autoload(), still running on the same activation, for the internal
function that raised it, and called the handler as its callback -- with no file or
line, and spl_autoload() and its caller traced twice.
--FILE--
<?php
function paths(string $s): string {
    $s = preg_replace('~(?:\b[A-Za-z]:)?/\S*/~', '', str_replace('\\', '/', $s));
    return str_replace(basename(__FILE__), 'main.php', $s);
}
function show(array $t): void {
    foreach ($t as $f) {
        echo paths('  ' . ($f['class'] ?? '') . ($f['type'] ?? '') . $f['function']
            . (isset($f['file']) ? ' ' . $f['file'] . ':' . $f['line'] : ' [internal]')), "\n";
    }
}
$dir = sys_get_temp_dir() . '/phl_spl_autoload_handler_' . getmypid();
@mkdir($dir);
$units = [
    'b1.inc' => "<?php class B1 {}\necho \$undef;\n",
    'b2.inc' => "<?php class B2 {}\necho \$undef;\n",
    'b3.inc' => "<?php class B3 {}\nhex2bin('a');\n",
    'b4.inc' => "<?php class B4 {}\n\$a = [];\necho \$a['k'];\n",
    'b5.inc' => "<?php declare(strict_types=1);\nclass B5 {}\necho \$undef;\n",
    'b6.inc' => "<?php class B6 {}\ninclude __DIR__ . '/b7.php';\n",
    'b7.php' => "<?php\necho \$undef;\n",
];
foreach ($units as $name => $code) {
    file_put_contents("$dir/$name", $code);
}
set_include_path($dir);
spl_autoload_register();
set_error_handler(function ($no, $str, $file, $line) {
    echo paths("$str @ $file:$line"), "\n";
    show(debug_backtrace());
    return true;
});
echo "-- class_exists\n";
class_exists('B1');
echo "-- new\n";
new B2;
echo "-- builtin inside the unit\n";
class_exists('B3');
echo "-- undefined key\n";
class_exists('B4');
echo "-- include inside the unit\n";
class_exists('B6');
echo "-- handler wants five\n";
set_error_handler(function (int $no, string $str, string $file, int $line, $five) { return true; });
try { class_exists('B5'); } catch (ArgumentCountError $e) { echo paths($e->getMessage()), "\n"; }
restore_error_handler();
foreach ($units as $name => $code) {
    unlink("$dir/$name");
}
rmdir($dir);
--EXPECT--
-- class_exists
Undefined variable $undef @ b1.inc:2
  {closure:main.php:28} b1.inc:2
  spl_autoload [internal]
  class_exists main.php:34
-- new
Undefined variable $undef @ b2.inc:2
  {closure:main.php:28} b2.inc:2
  spl_autoload main.php:36
-- builtin inside the unit
hex2bin(): Hexadecimal input string must have an even length @ b3.inc:2
  {closure:main.php:28} [internal]
  hex2bin b3.inc:2
  spl_autoload [internal]
  class_exists main.php:38
-- undefined key
Undefined array key "k" @ b4.inc:3
  {closure:main.php:28} b4.inc:3
  spl_autoload [internal]
  class_exists main.php:40
-- include inside the unit
Undefined variable $undef @ b7.php:2
  {closure:main.php:28} b7.php:2
  include b6.inc:2
  spl_autoload [internal]
  class_exists main.php:42
-- handler wants five
Too few arguments to function {closure:main.php:44}(), 4 passed in b5.inc on line 3 and exactly 5 expected
