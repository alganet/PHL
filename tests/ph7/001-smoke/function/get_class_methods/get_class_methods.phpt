--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_class_methods builtin basic checks
--FILE--
<?php
class GetClassMethodsA {
    public function f1(){}
    protected function f2(){}
    private function f3(){}
}
// public methods only: the list is filtered by the CALLING scope
$methods = get_class_methods('GetClassMethodsA');
echo in_array('f1', $methods) ? "ok\n" : "fail\n";
// f2 is protected and f3 private, so global scope sees neither — through an
// object argument as much as through a class name
$methods_obj = get_class_methods(new GetClassMethodsA);
echo (!in_array('f2', $methods_obj) && !in_array('f3', $methods_obj)) ? "ok\n" : "fail\n";
// a name that resolves to no class is a TypeError naming the type given
try {
    get_class_methods('NonExistent');
    echo "fail\n";
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
try {
    get_class_methods(42);
    echo "fail\n";
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
ok
ok
get_class_methods(): Argument #1 ($object_or_class) must be an object or a valid class name, string given
get_class_methods(): Argument #1 ($object_or_class) must be an object or a valid class name, int given
--CLEAN--
<?php
unset($methods, $methods_obj);
