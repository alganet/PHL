--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A reserved word is a legal segment of a qualified name in every name face
--DESCRIPTION--
php's lexer matches a qualified name as ONE token before it ever consults the keyword table,
so every reserved word is a legal SEGMENT of one -- `A\Default\Q`, `Default\Q`, `A\list` --
while a BARE reserved word is a keyword and no name at all. PHL rejected 58 of them in the
`use` statement and in every TYPE declaration; `new`, `extends`, `catch` and the rest already
accepted them, which is what made the gap look like a scanner inconsistency.
--FILE--
<?php
namespace NrwsA\Default {
    interface Face {}
    trait Tee { public function tm(){ return 'tm'; } }
    class Q implements Face { const K = 'K'; public static function m(){ return 'm'; } }
    function qf(){ return 'qf'; }
    const QC = 'QC';
}
namespace NrwsB {
    use NrwsA\Default\Q;
    use NrwsA\Default\{Face, Tee};
    use NrwsA\Default as ND;
    use function NrwsA\Default\qf;
    use const NrwsA\Default\QC;
    use NrwsA\Default\Q as Renamed;

    echo Q::K, "\n";
    echo ND\Q::m(), "\n";
    echo Renamed::K, "\n";
    echo qf(), "\n";
    echo QC, "\n";

    function nrwsParam(\NrwsA\Default\Q $q): string { return \get_class($q); }
    function nrwsReturn(): ND\Q { return new Q(); }
    function nrwsUnion(): ND\Q|Face { return new Q(); }
    class NrwsHold { public ?ND\Q $p = null; use Tee; }

    echo nrwsParam(new Q()), "\n";
    echo \get_class(nrwsReturn()), "\n";
    echo \get_class(nrwsUnion()), "\n";
    $h = new NrwsHold; $h->p = new Q();
    echo \get_class($h->p), "\n";
    echo $h->tm(), "\n";
    \var_dump(new Q() instanceof \NrwsA\Default\Face);
    echo (string)(new \ReflectionProperty('NrwsB\NrwsHold','p'))->getType(), "\n";
}
?>
--EXPECT--
K
m
K
qf
QC
NrwsA\Default\Q
NrwsA\Default\Q
NrwsA\Default\Q
NrwsA\Default\Q
tm
bool(true)
?NrwsA\Default\Q
--CLEAN--
<?php
