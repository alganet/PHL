--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A named argument to a built-in function or a Closure is refused where it is sent
--FILE--
<?php
/* php resolves a NAME at the send of its own argument whatever the callee is:
 * an unknown name, or one a positional argument already filled, throws before
 * any LATER argument runs and before a plain variable operand is read. A
 * built-in function binds by its declared parameter names, one that declares
 * none refuses every name, and a variadic one collects an unknown name for the
 * function itself to refuse. A Closure binds by the function it wraps. */
namespace Nhc;
set_error_handler(function ($no, $msg) { echo "  warning: $msg\n"; return true; });
function s($t) { echo "  ran $t\n"; return 'x'; }
function f($a = 1, $b = 2) { return "a=$a b=$b"; }
function t($label, $c) {
    echo "$label\n";
    try { $r = $c(); echo "  = ", var_export($r, true), "\n"; }
    catch (\Error $e) {
        echo "  ", get_class($e), ": ", $e->getMessage(), "\n";
        foreach ($e->getTrace() as $fr) {
            echo "    frame ", str_starts_with($fr['function'], '{closure') ? '{closure}' : $fr['function'], "\n";
        }
    }
}
$arr = [];
$cl = function ($a = 1, &$b = null) { return "cl a=$a"; };
$arrow = fn($a = 1) => "arrow a=$a";
t('built-in, undefined var then a call', fn() => strlen(zz: $undef, string: s('string')));
t('built-in, fully qualified', fn() => \strlen(zz: $undef, string: s('string')));
t('built-in, missing key', fn() => strlen(zz: $arr['nokey'], string: s('string')));
t('built-in, overwrites', fn() => str_repeat('a', 2, string: s('string')));
t('built-in, overwrites after a name', fn() => str_repeat('a', times: 2, string: s('string')));
t('built-in, by-reference earlier', fn() => preg_match('/a/', 'a', $m, zz: s('zz')));
t('built-in, declares none', fn() => time(zz: s('zz')));
t('built-in, variadic', fn() => sprintf('%s', zz: s('zz')));
t('built-in, bound by name', fn() => str_repeat(times: 2, string: 'ab'));
t('closure, undefined var then a call', fn() => $cl(zz: $undef, a: s('a')));
t('closure, overwrites', fn() => $cl(1, a: s('a')));
t('closure, by-reference formal', fn() => $cl(b: $fresh, a: 3));
t('arrow function', fn() => $arrow(zz: $undef, a: s('a')));
t('callable of a built-in', fn() => (strlen(...))(zz: $undef, string: s('string')));
t('callable of a function', fn() => (f(...))(zz: $undef, b: s('b')));
t('callable of a function, bound by name', fn() => (f(...))(b: 3));
--EXPECT--
built-in, undefined var then a call
  Error: Unknown named parameter $zz
    frame {closure}
    frame Nhc\t
built-in, fully qualified
  Error: Unknown named parameter $zz
    frame {closure}
    frame Nhc\t
built-in, missing key
  warning: Undefined array key "nokey"
  Error: Unknown named parameter $zz
    frame {closure}
    frame Nhc\t
built-in, overwrites
  ran string
  Error: Named parameter $string overwrites previous argument
    frame {closure}
    frame Nhc\t
built-in, overwrites after a name
  ran string
  Error: Named parameter $string overwrites previous argument
    frame {closure}
    frame Nhc\t
built-in, by-reference earlier
  ran zz
  Error: Unknown named parameter $zz
    frame {closure}
    frame Nhc\t
built-in, declares none
  ran zz
  Error: Unknown named parameter $zz
    frame {closure}
    frame Nhc\t
built-in, variadic
  ran zz
  ArgumentCountError: sprintf() does not accept unknown named parameters
    frame sprintf
    frame {closure}
    frame Nhc\t
built-in, bound by name
  = 'abab'
closure, undefined var then a call
  Error: Unknown named parameter $zz
    frame {closure}
    frame Nhc\t
closure, overwrites
  ran a
  Error: Named parameter $a overwrites previous argument
    frame {closure}
    frame Nhc\t
closure, by-reference formal
  = 'cl a=3'
arrow function
  Error: Unknown named parameter $zz
    frame {closure}
    frame Nhc\t
callable of a built-in
  Error: Unknown named parameter $zz
    frame {closure}
    frame Nhc\t
callable of a function
  Error: Unknown named parameter $zz
    frame {closure}
    frame Nhc\t
callable of a function, bound by name
  = 'a=1 b=3'
