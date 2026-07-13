--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ReflectionClass constants: getConstants order, filter, lookup
--FILE--
<?php
class ReflConstBase {
    const B_ONE = 1;
    const B_TWO = 'two';
}
class ReflConstKid extends ReflConstBase {
    const K_PUB = 'pub';
    protected const K_PROT = 'prot';
    private const K_PRIV = 'priv';
}

$rc = new ReflectionClass('ReflConstKid');
echo json_encode($rc->getConstants()), "\n";
echo json_encode($rc->getConstants(1)), "\n";
echo $rc->hasConstant('K_PUB') ? 'has' : 'no', "\n";
echo $rc->hasConstant('K_PRIV') ? 'has' : 'no', "\n";
echo $rc->hasConstant('NOPE') ? 'has' : 'no', "\n";
echo $rc->getConstant('B_TWO'), "\n";
?>
--EXPECT--
{"K_PUB":"pub","K_PROT":"prot","K_PRIV":"priv","B_ONE":1,"B_TWO":"two"}
{"K_PUB":"pub","B_ONE":1,"B_TWO":"two"}
has
has
no
two
