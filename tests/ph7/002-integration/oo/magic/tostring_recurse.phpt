--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Object __toString infinite recursion hits the native-nesting cap
--DESCRIPTION--
A magic method is invoked through a C->PHP callback trampoline (a native
VmByteCodeExec re-entry), not the OP_CALL trampoline — so unbounded __toString
recursion is bounded by the native-nesting cap (PH7_VM_CONFIG_NATIVE_DEPTH),
lowered here to 32 for a fast, deterministic fatal (BYTECODE.md stage 5).
phl-only: an engine-internal cap real php does not express.
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip phl-only: engine-internal native-nesting cap, not expressible in php'; ?>
--ENV--
PHL_MAX_NATIVE_DEPTH=32
--FILE--
<?php
class A {
    function __toString() {
        return (string)$this;
    }
}

$a = new A();
echo $a;
?>
--EXPECTF--
Error: Maximum native nesting depth reached in %s on line %d
Object
--CLEAN--
<?php
unset($a);
