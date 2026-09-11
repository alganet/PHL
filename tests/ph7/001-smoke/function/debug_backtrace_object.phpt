--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
debug_backtrace() attaches 'object' (PROVIDE_OBJECT) and honors IGNORE_ARGS
--FILE--
<?php
class DboOuter {
    public function run(): array { return (new DboInner())->go(7); }
}
class DboInner {
    public function go(int $n): array {
        return [
            'default' => debug_backtrace(),
            'ignore'  => debug_backtrace(DEBUG_BACKTRACE_PROVIDE_OBJECT | DEBUG_BACKTRACE_IGNORE_ARGS),
        ];
    }
}
$bt = (new DboOuter())->run();
// Frame 0 = DboInner::go (its object is the DboInner instance).
echo $bt['default'][0]['class'], $bt['default'][0]['type'], $bt['default'][0]['function'], "\n";
echo ($bt['default'][0]['object'] instanceof DboInner) ? "obj-inner\n" : "no\n";
// Frame 1 = DboOuter::run (object is the DboOuter instance).
echo ($bt['default'][1]['object'] instanceof DboOuter) ? "obj-outer\n" : "no\n";
// Default keeps args; IGNORE_ARGS drops them but keeps the object.
echo isset($bt['default'][0]['args']) ? "has-args\n" : "no-args\n";
echo isset($bt['ignore'][0]['args']) ? "has-args\n" : "no-args\n";
echo isset($bt['ignore'][0]['object']) ? "has-object\n" : "no-object\n";
?>
--EXPECT--
DboInner->go
obj-inner
obj-outer
has-args
no-args
has-object
--CLEAN--
<?php
