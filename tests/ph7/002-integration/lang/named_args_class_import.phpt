--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A `new` with a named argument still sees the file's class imports
--DESCRIPTION--
The named-argument map REPLACES the one the callee's namespace qualification
built, and it used to drop what that map recorded -- the original, unqualified
name literal, which is the only thing the `new` codegen can re-qualify with
CLASS imports. So `new Imported(x: 1)` resolved against the CURRENT namespace
and the class was not found, while the same `new` with positional arguments
worked. Found in respect/validation, which throws an imported exception with a
`previous:` argument.
--FILE--
<?php
namespace NacOther\Deep;

class Thing
{
    public function __construct(public $a = 'da', public $b = 'db')
    {
    }

    public static function make($a = 'sa', $b = 'sb'): self
    {
        return new self($a, $b);
    }
}

class Boom extends \Exception
{
}

function helper($a = 'ha', $b = 'hb'): string
{
    return "$a/$b";
}

namespace NacApp;

use NacOther\Deep as NacDeep;
use NacOther\Deep\Boom;
use NacOther\Deep\Thing;
use NacOther\Deep\Thing as NacAliased;

use function NacOther\Deep\helper;

function show(string $label, callable $f): void
{
    try {
        $r = $f();
    } catch (\Throwable $e) {
        $r = get_class($e) . ': ' . $e->getMessage();
    }
    printf("%-40s %s\n", $label, render($r));
}

function render(mixed $v): string
{
    if ($v instanceof Thing) {
        return get_class($v) . '(' . render($v->a) . ',' . render($v->b) . ')';
    }
    if (is_object($v)) {
        return get_class($v);
    }
    return var_export($v, true);
}

show('positional',                    fn() => new Thing('p'));
show('named only',                    fn() => new Thing(b: 'n'));
show('mixed',                         fn() => new Thing('p', b: 'n'));
show('a trailing comma',              fn() => new Thing(a: 'x',));
show('an alias',                      fn() => new NacAliased(b: 'n'));
show('through a namespace alias',     fn() => new NacDeep\Thing(b: 'n'));
show('fully qualified',               fn() => new \NacOther\Deep\Thing(b: 'n'));
show('nested in a named argument',    fn() => new Thing(a: new Thing(b: 'inner')));
show('inside an array',               fn() => [new Thing(b: 'n')][0]);
show('as another call\'s argument',   fn() => helper(a: new Thing(b: 'x')->b));
show('a spread',                      function () { $args = ['sp']; return new Thing(...$args); });
show('a named spread',                function () { $args = ['b' => 'sp']; return new Thing(...$args); });
show('a static call',                 fn() => NacAliased::make(b: 'n'));
show('an imported function',          fn() => helper(b: 'n'));
show('a global function',             fn() => strlen(string: 'abcd'));

/* The one that found it: an imported exception thrown from a catch block with
 * a `previous:` argument. */
show('an imported throw with previous:', function () {
    try {
        throw new \RuntimeException('inner');
    } catch (\Throwable $e) {
        throw new Boom('outer', previous: $e);
    }
});
--EXPECT--
positional                               NacOther\Deep\Thing('p','db')
named only                               NacOther\Deep\Thing('da','n')
mixed                                    NacOther\Deep\Thing('p','n')
a trailing comma                         NacOther\Deep\Thing('x','db')
an alias                                 NacOther\Deep\Thing('da','n')
through a namespace alias                NacOther\Deep\Thing('da','n')
fully qualified                          NacOther\Deep\Thing('da','n')
nested in a named argument               NacOther\Deep\Thing(NacOther\Deep\Thing('da','inner'),'db')
inside an array                          NacOther\Deep\Thing('da','n')
as another call's argument               'x/hb'
a spread                                 NacOther\Deep\Thing('sp','db')
a named spread                           NacOther\Deep\Thing('da','sp')
a static call                            NacOther\Deep\Thing('sa','n')
an imported function                     'ha/n'
a global function                        4
an imported throw with previous:         'NacOther\\Deep\\Boom: outer'
