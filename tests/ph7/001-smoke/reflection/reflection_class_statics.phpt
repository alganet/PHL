--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ReflectionClass static properties and default properties
--FILE--
<?php
class ReflStatBase {
    public static $bs = 'base-static';
    protected $bp = 'base-prop';
}
class ReflStatKid extends ReflStatBase {
    public static $ks = 10;
    private static $kpriv = 'secret';
    public $kp = array('a' => 1);
    public $noDefault;
}

$rc = new ReflectionClass('ReflStatKid');
echo json_encode($rc->getStaticProperties()), "\n";
echo $rc->getStaticPropertyValue('ks'), "\n";
echo $rc->getStaticPropertyValue('kpriv'), "\n";
echo $rc->getStaticPropertyValue('nope', 'fallback'), "\n";
try {
    $rc->getStaticPropertyValue('nope');
} catch (ReflectionException $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
$rc->setStaticPropertyValue('ks', 77);
echo ReflStatKid::$ks, "\n";
try {
    $rc->setStaticPropertyValue('nope', 1);
} catch (ReflectionException $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
echo json_encode($rc->getDefaultProperties()), "\n";
?>
--EXPECT--
{"ks":10,"kpriv":"secret","bs":"base-static"}
10
secret
fallback
ReflectionException: Property ReflStatKid::$nope does not exist
77
ReflectionException: Class ReflStatKid does not have a property named nope
{"ks":10,"kpriv":"secret","bs":"base-static","kp":{"a":1},"noDefault":null,"bp":"base-prop"}
