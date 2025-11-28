--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Interface test
--FILE--
<?php
interface IMyInterface {
    public function method1();
    public function method2($param);
}

class IMyClass implements IMyInterface {
    public function method1() {
        return "method1";
    }

    public function method2($param) {
        return "method2: " . $param;
    }
}

$instance = new IMyClass();
echo $instance->method1() . "\n";
echo $instance->method2("test") . "\n";
echo "Instance of interface: " . ($instance instanceof IMyInterface ? "yes" : "no") . "\n";
?>
--EXPECT--
method1
method2: test
Instance of interface: yes
--CLEAN--
<?php
// No cleanup needed
?>
