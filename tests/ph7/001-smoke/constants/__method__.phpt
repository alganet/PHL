--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: __METHOD__ magic constant
--FILE--
<?php
echo "__METHOD__ in global scope: '" . __METHOD__ . "'\n";
function MethConstFunc() {
    echo "__METHOD__ in function: '" . __METHOD__ . "'\n";
}
MethConstFunc();
class MethConstClass {
    public function methConstMethod() {
        echo "__METHOD__ in method: '" . __METHOD__ . "'\n";
    }
}
$obj = new MethConstClass();
$obj->methConstMethod();
?>
--EXPECTF--
%A__METHOD__ in global scope: ''%A__METHOD__ in function: 'MethConstFunc'%A__METHOD__ in method: 'MethConstClass::methConstMethod'%A
--CLEAN--
<?php
unset($obj);
