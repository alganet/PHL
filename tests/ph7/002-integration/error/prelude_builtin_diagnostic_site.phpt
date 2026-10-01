--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A diagnostic raised inside an internal function is reported at the call the program wrote
--FILE--
<?php
// scandir(), glob(), hex2bin(), str_increment(), tempnam() and ~19 more are
// INTERNAL functions: their bodies are not PHP code, so nothing they raise can
// report a position of its own. Every warning, every exception and every
// getFile()/getLine() belongs to the call the program wrote.
set_error_handler(function ($n, $s, $f, $l) {
    printf("[warn] %s @%s:%d\n", $s, basename($f), $l);
    return true;
});

hex2bin('a');
str_repeat('x', 1);
restore_error_handler();

function site(callable $cb) {
    try {
        $cb();
    } catch (Throwable $e) {
        printf("[throw] %s @%s:%d\n", get_class($e), basename($e->getFile()), $e->getLine());
    }
}
site(function () { scandir(''); });
site(function () { str_increment(''); });
site(function () { glob("a\0b"); });
site(function () { tempnam("a\0b", 'x'); });

// The trace still names the internal function at the same site.
try {
    scandir('');
} catch (Throwable $e) {
    echo $e->getTraceAsString(), "\n";
}

// A callback reached FROM one of these is ordinary user code and keeps its own
// position -- the walk stops at the first frame that is not internal.
class K {
    public function m()
    {
        return scandir('');
    }
}
site([new K, 'm']);
?>
--EXPECTF--
[warn] hex2bin(): Hexadecimal input string must have an even length @%s:11
[throw] ValueError @%s:22
[throw] ValueError @%s:23
[throw] ValueError @%s:24
[throw] ValueError @%s:25
#0 %s(29): scandir()
#1 {main}
[throw] ValueError @%s:39
--CLEAN--
<?php
