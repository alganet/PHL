--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A callback an internal function reached for has NO call site: no file, no line, and the builtin gets a frame
--DESCRIPTION--
php gives such a callback's frame neither `file` nor `line` -- `array_keys()` on it
answers `['function']` alone and the renderer prints `[internal function]: ` -- and
gives the INTERNAL function a frame of its own carrying the userland call site.
This engine collapsed the two into one, handing the callback the builtin's call
site, so a program walking a trace saw one frame where php has two.

php's two FORWARDS are folded away by the compiler and so keep the caller's own
frame -- but ONLY when it can prove the name is the global function. An
unqualified `call_user_func()` inside a NAMESPACE could resolve to a
namespace-local function, so php cannot fold it; nor can it fold a name held in a
variable. Every earlier probe of this rule was written in the global namespace,
which is why the forwards looked unconditionally folded.

The same fact decides the `, called in FILE on line N` tail of an argument
diagnostic, so both faces are pinned here.
--FILE--
<?php
namespace BtFrame;

// A callback an INTERNAL function reached for has no userland call site: php gives
// its frame NO file and NO line, and gives the builtin a frame of its own carrying
// the call site. php's two FORWARDS are the exception -- and only when the compiler
// can PROVE the name is the global function, which an unqualified call inside a
// namespace never is.
function shape(string $tag): void {
    $out = [];
    foreach (\debug_backtrace(DEBUG_BACKTRACE_IGNORE_ARGS) as $f) {
        $fn = $f['function'] ?? '?';
        if ($fn === 'BtFrame\shape') { continue; }
        $out[] = (isset($f['file']) ? 'AT' : 'NOFILE') . ':' . (\str_contains($fn, '{closure') ? '{closure}' : $fn);
    }
    echo $tag, ' => ', \implode(' | ', $out), "\n";
}
$cb = function () { shape('x'); };
$args = [];

$cb();                                        // a plain call
\call_user_func($cb);                         // qualified forward: php FOLDS it
\call_user_func_array($cb, $args);            // qualified forward: folded
call_user_func($cb);                          // unqualified INSIDE a namespace: not folded
call_user_func_array($cb, $args);             // unqualified inside a namespace: not folded
$n = '\call_user_func'; $n($cb);              // name in a variable: not folded
\array_map($cb, [1]);                         // an ordinary callback builtin
\array_filter([1], $cb);
\preg_replace_callback('/a/', $cb, 'a');

// The SAME fact drives the argument diagnostic's `, called in` tail.
class Rcv { public function m(int $i) {} }
$o = new Rcv();
foreach ([
    'qualified'   => static fn() => \call_user_func([$o, 'm'], 'x'),
    'unqualified' => static fn() => call_user_func([$o, 'm'], 'x'),
    'array_map'   => static fn() => \array_map([$o, 'm'], ['x']),
] as $k => $run) {
    try { $run(); } catch (\TypeError $e) {
        echo $k, ' => ', \str_contains($e->getMessage(), 'called in') ? 'HAS TAIL' : 'no tail', "\n";
    }
}
--EXPECT--
x => AT:{closure}
x => AT:{closure}
x => AT:{closure}
x => NOFILE:{closure} | AT:call_user_func
x => NOFILE:{closure} | AT:call_user_func_array
x => NOFILE:{closure} | AT:call_user_func
x => NOFILE:{closure} | AT:array_map
x => NOFILE:{closure} | AT:array_filter
x => NOFILE:{closure} | AT:preg_replace_callback
qualified => HAS TAIL
unqualified => no tail
array_map => no tail
