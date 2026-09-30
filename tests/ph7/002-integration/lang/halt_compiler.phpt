--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
__halt_compiler(): where the code stops and what the offset counts
--DESCRIPTION--
php's scanner stops at `__halt_compiler();` and never reads what follows as code
-- which is what lets a .phar carry a binary archive behind its stub. The three
rules that come with it: it is legal only at the outermost scope, its
parentheses and semicolon are part of the construct, and `__COMPILER_HALT_OFFSET__`
is the byte just past that semicolon.
--FILE--
<?php
$dir = sys_get_temp_dir() . '/phlt-halt-' . getmypid();
@mkdir($dir);
function put(string $name, string $body): string {
    global $dir;
    file_put_contents("$dir/$name", $body);
    return "$dir/$name";
}
function run(string $file): string {
    $out = [];
    exec(escapeshellarg(PHP_BINARY) . ' ' . escapeshellarg($file) . ' 2>&1', $out);
    /* The path is this run's, and the two engines never share one. Windows
     * reports it with backslashes while the name here was built with a `/`,
     * so both spellings have to go -- and so does the one with symlinks
     * resolved, which is how a diagnostic names it (macOS's /var is
     * /private/var). */
    $d = dirname($file);
    $r = realpath($d) ?: $d;
    $said = str_replace([$r, strtr($r, '/', '\\'), $d, strtr($d, '/', '\\')], '<dir>', implode("\n", $out));
    /* ...and the separator BELOW it is the platform's too. */
    return str_replace('<dir>\\', '<dir>/', $said);
}

echo "-- everything after it is DATA, not code and not output\n";
echo run(put('a.php', "<?php\necho \"before\\n\";\n__halt_compiler();\nthis is not php at all ){}\n"));
echo "\n";
echo run(put('b.php', "<?php\necho \"b\\n\";\n__HALT_COMPILER(); ?>\nRAW DATA HERE\n"));
echo "\n";
echo "-- and a chunk that follows it is data too\n";
echo run(put('c.php', "<?php __halt_compiler(); ?>\nplain text\n<?php echo \"never\\n\"; ?>\n"));
echo "\n";

echo "-- the offset is the byte just past the semicolon\n";
foreach ([
    'o1' => "<?php echo __COMPILER_HALT_OFFSET__;__halt_compiler();X",
    'o2' => "<?php echo __COMPILER_HALT_OFFSET__;__halt_compiler();\nX",
    'o3' => "<?php echo __COMPILER_HALT_OFFSET__;__halt_compiler(); ?>\r\nX",
    'o4' => "<?php echo __COMPILER_HALT_OFFSET__;__halt_compiler() ;\nX",
    'o5' => "#!/usr/bin/env php\n<?php echo __COMPILER_HALT_OFFSET__;__halt_compiler();X",
] as $name => $body) {
    $f = put("$name.php", $body);
    printf("%s size=%d offset=%s\n", $name, strlen($body), run($f));
}

echo "-- it may be read BEFORE the statement that sets it, and in an earlier chunk\n";
echo run(put('e1.php', "<?php \$x = __COMPILER_HALT_OFFSET__; ?>\n<?php echo \$x, \"\\n\"; __halt_compiler();\ntail")), "\n";

echo "-- a file with no halt has no such constant\n";
echo run(put('e2.php', "<?php var_dump(defined('__COMPILER_HALT_OFFSET__')); echo __COMPILER_HALT_OFFSET__;\n"));
echo "\n";
echo "-- ...and a file WITH one still answers defined() the way php does\n";
echo run(put('e3.php', "<?php var_dump(defined('__COMPILER_HALT_OFFSET__'));\n__halt_compiler();\n"));
echo "\n";

echo "-- only at the outermost scope\n";
echo run(put('s1.php', "<?php\nfunction f() { __halt_compiler(); }\n")), "\n";
echo run(put('s2.php', "<?php\nif (true) { __halt_compiler(); }\n")), "\n";
echo run(put('s3.php', "<?php\nclass C { }\n__halt_compiler();\n")), "\n";

echo "-- the parentheses and the semicolon belong to the construct\n";
echo run(put('p1.php', "<?php\necho \"p\\n\";\n__halt_compiler\n")), "\n";
echo run(put('p2.php', "<?php\n__halt_compiler(\n")), "\n";

foreach (glob("$dir/*") as $f) { @unlink($f); }
@rmdir($dir);
--EXPECT--
-- everything after it is DATA, not code and not output
before
b
-- and a chunk that follows it is data too

-- the offset is the byte just past the semicolon
o1 size=55 offset=54
o2 size=56 offset=54
o3 size=60 offset=54
o4 size=57 offset=55
o5 size=74 offset=73
-- it may be read BEFORE the statement that sets it, and in an earlier chunk
79
-- a file with no halt has no such constant
bool(false)
PHP Fatal error:  Uncaught Error: Undefined constant "__COMPILER_HALT_OFFSET__" in <dir>/e2.php:1
Stack trace:
#0 {main}
  thrown in <dir>/e2.php on line 1
-- ...and a file WITH one still answers defined() the way php does
bool(false)
-- only at the outermost scope
PHP Fatal error:  __HALT_COMPILER() can only be used from the outermost scope in <dir>/s1.php on line 2
PHP Fatal error:  __HALT_COMPILER() can only be used from the outermost scope in <dir>/s2.php on line 2

-- the parentheses and the semicolon belong to the construct
PHP Parse error:  syntax error, unexpected end of file, expecting "(" in <dir>/p1.php on line 4
PHP Parse error:  Unclosed '(' on line 2 in <dir>/p2.php on line 3
