--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ReflectionClass member existence checks
--FILE--
<?php
class ReflMemBase {
    public function basePub() {}
    private function basePriv() {}
    protected $baseProp;
}
class ReflMemKid extends ReflMemBase {
    public function kidDo() {}
    private $kidProp;
}

$rc = new ReflectionClass('ReflMemKid');
echo $rc->hasMethod('kidDo') ? 'y' : 'n', "\n";
echo $rc->hasMethod('KIDDO') ? 'y' : 'n', "\n";
echo $rc->hasMethod('basePub') ? 'y' : 'n', "\n";
echo $rc->hasMethod('basePriv') ? 'y' : 'n', "\n";
echo $rc->hasMethod('nope') ? 'y' : 'n', "\n";
echo $rc->hasProperty('kidProp') ? 'y' : 'n', "\n";
echo $rc->hasProperty('KidProp') ? 'y' : 'n', "\n";
echo $rc->hasProperty('baseProp') ? 'y' : 'n', "\n";
echo $rc->hasProperty('nope') ? 'y' : 'n', "\n";
?>
--EXPECT--
y
y
y
y
n
y
n
y
n
