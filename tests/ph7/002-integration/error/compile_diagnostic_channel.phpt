--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A COMPILE-time diagnostic owes error_reporting(), error_get_last() and the handler
--DESCRIPTION--
The compiler's diagnostics went out on a channel of their own that asked none
of the three questions a runtime diagnostic asks, so `error_reporting(0)` did
not silence one, `error_get_last()` never saw one, and `set_error_handler()`
was never offered one. A library that probes an optional dependency with
`error_reporting(0); class_exists('X'); error_reporting($prev);` -- monolog's
suite does exactly that for php-console -- got the compiler's warning printed
into its output anyway.

Three answers, all swept out of php 8.5 rather than assumed:

* the SEVERITY is per diagnostic, and the compiler has bits of its own.
  `Private methods cannot be final`, `Octal escape sequence overflow` and
  `declare(encoding=...) ignored` are E_COMPILE_WARNING (128); the
  `"continue" targeting switch` and magic-visibility rules are a plain
  E_WARNING (2) raised at compile time. `E_ALL & ~E_COMPILE_WARNING` hides one
  set and leaves the other standing.
* only the E_WARNING half reaches a user handler -- E_COMPILE_WARNING,
  E_COMPILE_ERROR and E_PARSE are on php's own exclusion list.
* error_get_last() records what reached DEFAULT processing: masked still
  counts, claimed by a handler does not.

The last block is why the emitter copies its message first: the text it is
handed is the code generator's one-message buffer, and a handler that
compiles anything -- an include, an eval, a FAILING eval -- resets exactly
that buffer under it.

One window is left, and it is structural: `ph7_compile_file` CREATES the VM,
so the MAIN script's own compile runs before the host installs a reporting
level and its diagnostics are reported unmasked.
--FILE--
<?php
$dir = sys_get_temp_dir() . '/phl-compile-diag-' . getmypid();
@mkdir($dir, 0777, true);
$n = 0;

/* Each unit is a file compiled at RUN time, so the VM's error_reporting is
 * already installed when its diagnostics are raised. */
$units = [
    'E_COMPILE_WARNING (final private)' =>
        'class C%N% { private final function __clone() {} }',
    'E_COMPILE_WARNING (octal escape)' =>
        '$GLOBALS["o%N%"] = "\\400";',
    'E_WARNING (continue in switch)' =>
        'function w%N%() { foreach ([1] as $x) { switch ($x) { case 1: continue; } } }',
    'E_WARNING (magic visibility)' =>
        'class M%N% { private function __get($k) { return 1; } }',
];

function unitFile(string $dir, string $src, int $n): string
{
    $path = "$dir/u$n.php";
    file_put_contents($path, "<?php\n" . str_replace('%N%', (string) $n, $src) . "\n");

    return $path;
}

function lastOf(): string
{
    $e = error_get_last();

    return $e === null ? 'none' : $e['type'] . ' ' . $e['message'];
}

/* 1. error_reporting() masks a compile diagnostic exactly as it masks a
 *    runtime one, and the two compiler bits are separable. */
foreach ($units as $label => $src) {
    foreach ([
        'E_ALL'                   => E_ALL,
        '~E_WARNING'              => E_ALL & ~E_WARNING,
        '~E_COMPILE_WARNING'      => E_ALL & ~E_COMPILE_WARNING,
        'silenced'                => 0,
    ] as $maskName => $mask) {
        $f = unitFile($dir, $src, ++$n);
        $prev = error_reporting($mask);
        require $f;
        error_reporting($prev);
        printf("%-34s %-20s last=%s\n", $label, $maskName, lastOf());
        unlink($f);
    }
}

/* 2. A user error handler sees a compile-time E_WARNING and never sees an
 *    E_COMPILE_WARNING -- php's own exclusion list. A claimed diagnostic
 *    leaves error_get_last() alone. */
foreach ($units as $label => $src) {
    $f = unitFile($dir, $src, ++$n);
    $seen = [];
    set_error_handler(function ($no, $msg) use (&$seen) {
        $seen[] = "$no $msg";

        return true;
    });
    require $f;
    restore_error_handler();
    printf("%-34s handler=%-6s last=%s\n", $label, $seen === [] ? 'no' : 'yes', lastOf());
    if ($seen !== []) {
        echo "  -> ", implode(' | ', $seen), "\n";
    }
    unlink($f);
}

@rmdir($dir);

/* 3. The handler may itself COMPILE -- an include, an eval, a failing eval.
 *    The diagnostic it was handed is the code generator's one-message buffer,
 *    which every one of those resets. */
$dir2 = sys_get_temp_dir() . '/phl-compile-diag2-' . getmypid();
@mkdir($dir2, 0777, true);
file_put_contents("$dir2/inner.php", "<?php\nclass ReInner { private final function __clone() {} }\n");
file_put_contents("$dir2/unit.php", "<?php\nfunction reU() { foreach ([1] as \$x) { switch (\$x) { case 1: continue; } } }\n");
set_error_handler(function ($no, $msg) use ($dir2) {
    require "$dir2/inner.php";
    eval('$q = 1 + 1;');
    try {
        eval('echo 1 foo;');
    } catch (ParseError $e) {
        echo "  nested parse: ", $e->getMessage(), "\n";
    }
    echo "  handler still holds: $no $msg\n";

    return true;
});
require "$dir2/unit.php";
restore_error_handler();
echo "after a compiling handler: ", lastOf(), "\n";
unlink("$dir2/inner.php");
unlink("$dir2/unit.php");
@rmdir($dir2);
--EXPECTF--
PHP Warning:  Private methods cannot be final as they are never overridden by other classes in %s on line 2
E_COMPILE_WARNING (final private)  E_ALL                last=128 Private methods cannot be final as they are never overridden by other classes
PHP Warning:  Private methods cannot be final as they are never overridden by other classes in %s on line 2
E_COMPILE_WARNING (final private)  ~E_WARNING           last=128 Private methods cannot be final as they are never overridden by other classes
E_COMPILE_WARNING (final private)  ~E_COMPILE_WARNING   last=128 Private methods cannot be final as they are never overridden by other classes
E_COMPILE_WARNING (final private)  silenced             last=128 Private methods cannot be final as they are never overridden by other classes
PHP Warning:  Octal escape sequence overflow \400 is greater than \377 in %s on line 2
E_COMPILE_WARNING (octal escape)   E_ALL                last=128 Octal escape sequence overflow \400 is greater than \377
PHP Warning:  Octal escape sequence overflow \400 is greater than \377 in %s on line 2
E_COMPILE_WARNING (octal escape)   ~E_WARNING           last=128 Octal escape sequence overflow \400 is greater than \377
E_COMPILE_WARNING (octal escape)   ~E_COMPILE_WARNING   last=128 Octal escape sequence overflow \400 is greater than \377
E_COMPILE_WARNING (octal escape)   silenced             last=128 Octal escape sequence overflow \400 is greater than \377
PHP Warning:  "continue" targeting switch is equivalent to "break". Did you mean to use "continue 2"? in %s on line 2
E_WARNING (continue in switch)     E_ALL                last=2 "continue" targeting switch is equivalent to "break". Did you mean to use "continue 2"?
E_WARNING (continue in switch)     ~E_WARNING           last=2 "continue" targeting switch is equivalent to "break". Did you mean to use "continue 2"?
PHP Warning:  "continue" targeting switch is equivalent to "break". Did you mean to use "continue 2"? in %s on line 2
E_WARNING (continue in switch)     ~E_COMPILE_WARNING   last=2 "continue" targeting switch is equivalent to "break". Did you mean to use "continue 2"?
E_WARNING (continue in switch)     silenced             last=2 "continue" targeting switch is equivalent to "break". Did you mean to use "continue 2"?
PHP Warning:  The magic method M13::__get() must have public visibility in %s on line 2
E_WARNING (magic visibility)       E_ALL                last=2 The magic method M13::__get() must have public visibility
E_WARNING (magic visibility)       ~E_WARNING           last=2 The magic method M14::__get() must have public visibility
PHP Warning:  The magic method M15::__get() must have public visibility in %s on line 2
E_WARNING (magic visibility)       ~E_COMPILE_WARNING   last=2 The magic method M15::__get() must have public visibility
E_WARNING (magic visibility)       silenced             last=2 The magic method M16::__get() must have public visibility
PHP Warning:  Private methods cannot be final as they are never overridden by other classes in %s on line 2
E_COMPILE_WARNING (final private)  handler=no     last=128 Private methods cannot be final as they are never overridden by other classes
PHP Warning:  Octal escape sequence overflow \400 is greater than \377 in %s on line 2
E_COMPILE_WARNING (octal escape)   handler=no     last=128 Octal escape sequence overflow \400 is greater than \377
E_WARNING (continue in switch)     handler=yes    last=128 Octal escape sequence overflow \400 is greater than \377
  -> 2 "continue" targeting switch is equivalent to "break". Did you mean to use "continue 2"?
E_WARNING (magic visibility)       handler=yes    last=128 Octal escape sequence overflow \400 is greater than \377
  -> 2 The magic method M20::__get() must have public visibility
PHP Warning:  Private methods cannot be final as they are never overridden by other classes in %s on line 2
  nested parse: syntax error, unexpected identifier "foo", expecting "," or ";"
  handler still holds: 2 "continue" targeting switch is equivalent to "break". Did you mean to use "continue 2"?
after a compiling handler: 128 Private methods cannot be final as they are never overridden by other classes
