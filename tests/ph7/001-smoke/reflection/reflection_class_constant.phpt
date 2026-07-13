--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ReflectionClassConstant basics
--FILE--
<?php
class ReflCConst {
    const CPUB = 1;
    protected const CPROT = 'two';
    private const CPRIV = array(3);
    final const CFIN = 4;
}

$rcc = new ReflectionClassConstant('ReflCConst', 'CPROT');
echo $rcc->name, ' @ ', $rcc->class, "\n";
echo $rcc->getValue(), "\n";
echo $rcc->getModifiers(), ':', implode(',', Reflection::getModifierNames($rcc->getModifiers())), "\n";
echo $rcc->isProtected() ? 'prot' : 'x', "\n";
echo $rcc->getDeclaringClass()->name, "\n";
$rcf = new ReflectionClassConstant('ReflCConst', 'CFIN');
echo $rcf->isFinal() ? 'final' : 'notfinal', "\n";
echo $rcf->isPublic() ? 'pub' : 'x', "\n";
echo $rcf->isEnumCase() ? 'case' : 'not-case', "\n";
$rcp = new ReflectionClassConstant('ReflCConst', 'CPRIV');
echo json_encode($rcp->getValue()), "\n";
echo $rcp->isPrivate() ? 'priv' : 'x', "\n";
try {
    new ReflectionClassConstant('ReflCConst', 'NOPE');
} catch (ReflectionException $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
$rc = new ReflectionClass('ReflCConst');
$g = $rc->getReflectionConstant('CPUB');
echo get_class($g), ':', $g->name, "\n";
echo $rc->getReflectionConstant('NOPE') === false ? 'false' : 'obj', "\n";
foreach ($rc->getReflectionConstants() as $c) { echo $c->name, ','; } echo "\n";
foreach ($rc->getReflectionConstants(ReflectionClassConstant::IS_PUBLIC) as $c) { echo $c->name, ','; } echo "\n";
?>
--EXPECT--
CPROT @ ReflCConst
two
2:protected
prot
ReflCConst
final
pub
not-case
[3]
priv
ReflectionException: Constant ReflCConst::NOPE does not exist
ReflectionClassConstant:CPUB
false
CPUB,CPROT,CPRIV,CFIN,
CPUB,CFIN,
