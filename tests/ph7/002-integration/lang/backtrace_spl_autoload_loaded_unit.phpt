--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A unit spl_autoload() loads is traced under spl_autoload() and its caller, not as an include
--DESCRIPTION--
The default autoloader, spl_autoload(), compiles and runs the file it finds itself:
php's trace from inside that file shows spl_autoload() as a frame -- with no file
or line when a builtin such as class_exists() asked for the class, and that builtin
at the userland call site -- and no include frame at all. This engine traced the
load as an include named spl_autoload(), at the call site, with the file path as
its argument, and dropped the builtin that triggered it.

It also took every internal call the loaded unit makes for one reached from inside
spl_autoload(), because both run in the same activation: a str_repeat() that threw
at the top level of the file, or an array_map() running a callback there, lost its
own file and line.
--FILE--
<?php
function paths(string $s): string {
    $s = preg_replace('~(?:\b[A-Za-z]:)?/\S*/~', '', str_replace('\\', '/', $s));
    return str_replace(basename(__FILE__), 'main.php', $s);
}
function show(array $t): void {
    foreach ($t as $f) {
        echo paths('  ' . ($f['class'] ?? '') . ($f['type'] ?? '') . $f['function'] . '('
            . implode(',', array_map(fn($a) => is_string($a) ? $a : get_debug_type($a), $f['args'] ?? [])) . ')'
            . (isset($f['file']) ? ' ' . $f['file'] . ':' . $f['line'] : ' [internal]')), "\n";
    }
}
function tr(Throwable $e): void {
    echo paths($e->getTraceAsString()), "\n";
}

$dir = sys_get_temp_dir() . '/phl_spl_autoload_trace_' . getmypid();
@mkdir($dir);
$units = [
    'a1.inc' => "<?php class A1 {}\nshow(debug_backtrace());\ntry { str_repeat('x', -1); } catch (ValueError \$e) { tr(\$e); }\ntr(new Exception);\n",
    'a2.inc' => "<?php class A2 {}\nshow(debug_backtrace()); tr(new Exception);\n",
    'a3.inc' => "<?php class A3 {}\nshow(debug_backtrace()); tr(new Exception);\n",
    'a4.inc' => "<?php class A4 {}\nclass_exists('A5');\n",
    'a5.inc' => "<?php class A5 {}\nshow(debug_backtrace()); tr(new Exception);\n",
    'a6.inc' => "<?php class A6 {}\narray_map(function (\$x) { show(debug_backtrace()); tr(new Exception); }, [1]);\n",
    'a7.inc' => "<?php class A7 {}\ninclude __DIR__ . '/a8.php';\n",
    'a8.php' => "<?php\nshow(debug_backtrace()); tr(new Exception);\n",
    'a9.inc' => "<?php interface A9 {}\nshow(debug_backtrace()); tr(new Exception);\n",
];
foreach ($units as $name => $code) {
    file_put_contents("$dir/$name", $code);
}
set_include_path($dir);
spl_autoload_register();

echo "-- class_exists\n";
class_exists('A1');
echo "-- new\n";
new A2;
echo "-- called directly\n";
spl_autoload('A3');
echo "-- loaded unit asks for another\n";
class_exists('A4');
echo "-- callback inside the loaded unit\n";
class_exists('A6');
echo "-- include inside the loaded unit\n";
class_exists('A7');
echo "-- from a function\n";
function f() { return interface_exists('A9'); }
f();

foreach ($units as $name => $code) {
    unlink("$dir/$name");
}
rmdir($dir);
?>
--EXPECT--
-- class_exists
  spl_autoload(A1) [internal]
  class_exists(A1) main.php:37
#0 a1.inc(3): str_repeat()
#1 [internal function]: spl_autoload()
#2 main.php(37): class_exists()
#3 {main}
#0 [internal function]: spl_autoload()
#1 main.php(37): class_exists()
#2 {main}
-- new
  spl_autoload(A2) main.php:39
#0 main.php(39): spl_autoload()
#1 {main}
-- called directly
  spl_autoload(A3) main.php:41
#0 main.php(41): spl_autoload()
#1 {main}
-- loaded unit asks for another
  spl_autoload(A5) [internal]
  class_exists(A5) a4.inc:2
  spl_autoload(A4) [internal]
  class_exists(A4) main.php:43
#0 [internal function]: spl_autoload()
#1 a4.inc(2): class_exists()
#2 [internal function]: spl_autoload()
#3 main.php(43): class_exists()
#4 {main}
-- callback inside the loaded unit
  {closure:a6.inc:2}(int) [internal]
  array_map(Closure,array) a6.inc:2
  spl_autoload(A6) [internal]
  class_exists(A6) main.php:45
#0 [internal function]: {closure:a6.inc:2}()
#1 a6.inc(2): array_map()
#2 [internal function]: spl_autoload()
#3 main.php(45): class_exists()
#4 {main}
-- include inside the loaded unit
  include() a7.inc:2
  spl_autoload(A7) [internal]
  class_exists(A7) main.php:47
#0 a7.inc(2): include()
#1 [internal function]: spl_autoload()
#2 main.php(47): class_exists()
#3 {main}
-- from a function
  spl_autoload(A9) [internal]
  interface_exists(A9) main.php:49
  f() main.php:50
#0 [internal function]: spl_autoload()
#1 main.php(49): interface_exists()
#2 main.php(50): f()
#3 {main}
