--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Object cast to array
--DESCRIPTION--
php mangles the keys of non-public properties in an (array) cast: private becomes
"\0DeclaringClass\0name" and protected "\0*\0name". This used to assert PHL's bare names
behind a bare zend_version skip, so the divergence never ran under the oracle; it is
cross-engine now (the NULs are rendered as "|" to keep the expectation readable).
--FILE--
<?php
class ObjectCastToArrayTestClass {
    public $publicVar = 'public';
    private $privateVar = 'private';
    protected $protectedVar = 'protected';
}

$obj = new ObjectCastToArrayTestClass();
$array = (array) $obj;
foreach ($array as $key => $value) {
    echo str_replace("\0", '|', $key), ' => ', $value, "\n";
}
var_dump(count($array), isset($array['privateVar']));
?>
--EXPECT--
publicVar => public
|ObjectCastToArrayTestClass|privateVar => private
|*|protectedVar => protected
int(3)
bool(false)
--CLEAN--
<?php
unset($obj, $array);
