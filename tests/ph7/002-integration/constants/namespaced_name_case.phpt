--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A constant's namespace part matches without regard to case, its final segment exactly, and a lookup drops one leading backslash
--FILE--
<?php
// php's constant table folds only the NAMESPACE part of a name: everything up
// to and including the last backslash is compared case-insensitively, the
// segment after it byte for byte. A lookup -- defined(), constant(), a literal
// -- also drops one leading backslash; define() does not, which is why the
// constant define('\Lead\XX') makes can never be named again.
namespace Nn\Mm {
    const KK = 41;
}
namespace {
    define('Aa\Bb\DEE', 1);
    define('GLOB', 2);
    foreach ([
        'Aa\Bb\DEE', 'aa\bb\DEE', 'AA\BB\DEE', 'aA\bB\DEE',
        'Aa\Bb\dee', 'Aa\Bb\DEe', 'aa\bb\dee',
        '\Aa\Bb\DEE', '\aa\BB\DEE',
        'Aa\\\\Bb\DEE',
        'GLOB', 'glob', 'Glob', '\GLOB', '\glob',
    ] as $p) {
        printf("%-14s %s\n", str_replace("\\", "/", $p), var_export(defined($p), true));
    }
    echo 'constant: ', var_export(constant('aa\BB\DEE'), true), "\n";

    // define() keeps the name it was handed, so a leading backslash makes an
    // unreachable constant -- and get_defined_constants() still reports the
    // spelling it was DECLARED with, not the folded key the table matches on.
    define('\Lead\XX', 3);
    echo 'Lead/XX: ', var_export(defined('Lead\XX'), true),
         ' ', var_export(defined('\Lead\XX'), true), "\n";
    $names = array_keys(get_defined_constants(true)['user']);
    sort($names);
    foreach ($names as $k) {
        echo "user: ", str_replace("\\", "/", $k), "\n";
    }
}
// The compile-time literal and the `use const` import take the same rule.
namespace Other {
    use const nN\mM\KK as AL;
    echo \nN\mM\KK, " ", AL, " ", \NN\MM\KK, "\n";
}
?>
--EXPECT--
Aa/Bb/DEE      true
aa/bb/DEE      true
AA/BB/DEE      true
aA/bB/DEE      true
Aa/Bb/dee      false
Aa/Bb/DEe      false
aa/bb/dee      false
/Aa/Bb/DEE     true
/aa/BB/DEE     true
Aa//Bb/DEE     false
GLOB           true
glob           false
Glob           false
/GLOB          true
/glob          false
constant: 1
Lead/XX: false false
user: Aa/Bb/DEE
user: GLOB
user: Nn/Mm/KK
user: /Lead/XX
41 41 41
