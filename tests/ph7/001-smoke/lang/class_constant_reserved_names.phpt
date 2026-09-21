--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Class constants may be named with any reserved word except `class`
--FILE--
<?php
// php 8 accepts every reserved word as a class-constant name: the name is only ever
// addressed through `C::name`, where no keyword is ambiguous.
class CcrnWords
{
    const list = 'list';
    const match = 'match';
    const fn = 'fn';
    const print = 'print';
    const array = 'array';
    const function = 'function';
    const if = 'if';
    const foreach = 'foreach';
    const static = 'static';
    const self = 'self';
    const parent = 'parent';
    const isset = 'isset';
    const empty = 'empty';
    const eval = 'eval';
    const unset = 'unset';
    const int = 'int';
    const string = 'string';
    const object = 'object';
    const namespace = 'namespace';
    const use = 'use';
    const endif = 'endif';
    const new = 'new';
    const clone = 'clone';
    const instanceof = 'instanceof';
}
echo CcrnWords::list, ' ', CcrnWords::match, ' ', CcrnWords::fn, ' ', CcrnWords::print, "\n";
echo CcrnWords::array, ' ', CcrnWords::function, ' ', CcrnWords::if, ' ', CcrnWords::foreach, "\n";
echo CcrnWords::static, ' ', CcrnWords::self, ' ', CcrnWords::parent, "\n";
echo CcrnWords::isset, ' ', CcrnWords::empty, ' ', CcrnWords::eval, ' ', CcrnWords::unset, "\n";
echo CcrnWords::int, ' ', CcrnWords::string, ' ', CcrnWords::object, "\n";
echo CcrnWords::namespace, ' ', CcrnWords::use, ' ', CcrnWords::endif, "\n";
echo CcrnWords::new, ' ', CcrnWords::clone, ' ', CcrnWords::instanceof, "\n";

// true/false/null are reserved GLOBAL constant names, but they name a class constant
// just like any other reserved word.
class CcrnLiterals
{
    const true = 't';
    const false = 'f';
    const null = 'n';
}
echo CcrnLiterals::true, CcrnLiterals::false, CcrnLiterals::null, "\n";

// Every name in a multi-declaration, with visibility, `final` and a type.
class CcrnShapes
{
    public const list = 1, match = 2, print = 3;
    private const string = 4;
    final public const int foreach = 5;
    public static function readPrivate(): int { return self::string; }
}
echo CcrnShapes::list, CcrnShapes::match, CcrnShapes::print, CcrnShapes::readPrivate(),
    CcrnShapes::foreach, "\n";

// Interfaces, enums and inheritance carry them too.
interface CcrnIface { const while = 'w'; }
class CcrnImpl implements CcrnIface {}
echo CcrnImpl::while, "\n";

enum CcrnEnum: string
{
    const default = 'd';
    case list = 'L';
    case match = 'M';
}
echo CcrnEnum::default, ' ', CcrnEnum::list->value, CcrnEnum::match->name, "\n";

// Dynamic and reflective reads see the same names.
$ccrnCls = 'CcrnWords';
echo constant('CcrnWords::list'), ' ', $ccrnCls::match, "\n";
var_dump((new ReflectionClass('CcrnLiterals'))->getConstant('true'),
    array_key_exists('match', (new ReflectionClass('CcrnShapes'))->getConstants()));

// `class` alone stays reserved for `C::class` — reading it still fetches the name.
echo CcrnWords::class, "\n";
?>
--EXPECT--
list match fn print
array function if foreach
static self parent
isset empty eval unset
int string object
namespace use endif
new clone instanceof
tfn
12345
w
d Lmatch
list match
string(1) "t"
bool(true)
CcrnWords
--CLEAN--
<?php
