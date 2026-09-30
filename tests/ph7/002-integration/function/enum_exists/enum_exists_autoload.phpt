--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
enum_exists() honours its $autoload argument: false must not run the loader
--FILE--
<?php
spl_autoload_register(function ($c) {
    echo "autoload($c)\n";
    if ($c === 'EeaLazyEnum') {
        eval('enum EeaLazyEnum: string { case A = "a"; }');
    }
});
// $autoload = false is the one spelling that asks "is it loaded ALREADY", and
// its whole point is that the loader must not run.
var_dump(enum_exists('EeaLazyEnum', false));
var_dump(enum_exists('EeaLazyEnum'));
var_dump(enum_exists('EeaLazyEnum', false));
// A leading '\' is the global anchor on the no-autoload road too.
var_dump(enum_exists('\\EeaLazyEnum', false));
// A name nothing declares stays false either way, and only the autoloading form looks.
var_dump(enum_exists('EeaMissing', false));
var_dump(enum_exists('EeaMissing'));
// A plain class is not an enum, whichever road asks.
class EeaPlain {}
var_dump(enum_exists('EeaPlain', false), enum_exists('EeaPlain'));
?>
--EXPECT--
bool(false)
autoload(EeaLazyEnum)
bool(true)
bool(true)
bool(true)
bool(false)
autoload(EeaMissing)
bool(false)
bool(false)
bool(false)
