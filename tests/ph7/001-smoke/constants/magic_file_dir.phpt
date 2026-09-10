--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: __FILE__ and __DIR__ are compile-time and mutually consistent
--FILE--
<?php
// __DIR__ is the directory part of __FILE__, resolved at compile time.
echo (int)(__DIR__ === dirname(__FILE__)), "\n";
echo (int)(__FILE__ === __DIR__ . DIRECTORY_SEPARATOR . basename(__FILE__)), "\n";
// Same holds inside a function body (still the compiling file).
function mfd_check() { return (int)(__DIR__ === dirname(__FILE__)); }
echo mfd_check(), "\n";
// Inside an included file, __FILE__/__DIR__ reflect that file, not the includer.
$inc = tempnam(sys_get_temp_dir(), 'mfd');
file_put_contents($inc, '<?php return [__FILE__, __DIR__];');
$r = include $inc;
echo (int)($r[1] === dirname($r[0])), "\n";
echo (int)(basename($r[0]) === basename($inc)), "\n";
unlink($inc);
?>
--EXPECT--
1
1
1
1
1
--CLEAN--
<?php
