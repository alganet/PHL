--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ReflectionConstant, ReflectionExtension, ReflectionEnum stubs, ReflectionReference
--FILE--
<?php
define('REFL_LT_DEF', 10);
const REFL_LT_CONST = 'c';

$rc = new ReflectionConstant('REFL_LT_DEF');
echo $rc->getName(), '=', $rc->getValue(), "\n";
echo $rc->getShortName(), ' ns=[', $rc->getNamespaceName(), "]\n";
echo $rc->isDeprecated() ? 'dep' : 'not-dep', "\n";
$rc2 = new ReflectionConstant('REFL_LT_CONST');
echo $rc2->getValue(), "\n";
echo (new ReflectionConstant('PHP_INT_MAX'))->getValue() === PHP_INT_MAX ? 'intmax-ok' : 'intmax-bad', "\n";
try {
    new ReflectionConstant('REFL_LT_NOPE');
} catch (ReflectionException $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}

$re = new ReflectionExtension('core');
echo $re->getName(), "\n";
try {
    new ReflectionExtension('reflltnosuch');
} catch (ReflectionException $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
try {
    new ReflectionZendExtension('reflltnosuch');
} catch (ReflectionException $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}

class ReflLtPlain {}
try {
    new ReflectionEnum('ReflLtPlain');
} catch (ReflectionException $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
try {
    new ReflectionEnum('ReflLtNoCls');
} catch (ReflectionException $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}

$reflLtArr = array(1, 2);
$reflLtRef = &$reflLtArr[0];
$ref = ReflectionReference::fromArrayElement($reflLtArr, 0);
echo $ref === null ? 'null' : get_class($ref), "\n";
echo ReflectionReference::fromArrayElement($reflLtArr, 1) === null ? 'null' : 'obj', "\n";
echo is_string($ref->getId()) ? 'id-str' : 'id-other', "\n";
$ref2 = ReflectionReference::fromArrayElement($reflLtArr, 0);
echo $ref->getId() === $ref2->getId() ? 'id-stable' : 'id-diff', "\n";

echo json_encode((new ReflectionClass('Exception'))->getExtension()->getName()), "\n";
echo (new ReflectionClass('ReflLtPlain'))->getExtension() === null ? 'null' : 'ext', "\n";
echo (new ReflectionFunction('strlen'))->getExtension()->getName(), "\n";
?>
--EXPECT--
REFL_LT_DEF=10
REFL_LT_DEF ns=[]
not-dep
c
intmax-ok
ReflectionException: Constant "REFL_LT_NOPE" does not exist
Core
ReflectionException: Extension "reflltnosuch" does not exist
ReflectionException: Zend Extension "reflltnosuch" does not exist
ReflectionException: Class "ReflLtPlain" is not an enum
ReflectionException: Class "ReflLtNoCls" does not exist
ReflectionReference
null
id-str
id-stable
"Core"
null
Core
