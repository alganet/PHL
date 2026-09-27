--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php's own attributes are judged where the declaration compiles
--DESCRIPTION--
php's seven internal attribute classes each carry a target mask AND a validator,
and the engine runs both where the declaration COMPILES: `#[\Attribute] interface
I {}`, `#[\Override] function f() {}` and `#[\NoDiscard] class X {}` are fatals
at the line they sit on, before anything else in the file runs. PHL ran every
attribute check at the reflection call instead, so a misplaced one was silent
until something asked -- and for the five classes added this session, silent
forever.

A USERLAND attribute is the other half of the same rule and stays as it was:
php checks its mask only at `newInstance()`, so `#[MyClassAttr] function f() {}`
still compiles. That is why the table here is closed rather than a lookup of
whatever class the name resolves to.
--FILE--
<?php
$cases = [
    'Attribute on a class' => '#[\Attribute] class A {}',
    'Attribute on a final class' => '#[\Attribute] final class A {}',
    'Attribute on an abstract one' => '#[\Attribute] abstract class A {}',
    'Attribute on an interface' => '#[\Attribute] interface I {}',
    'Attribute on a trait' => '#[\Attribute] trait T {}',
    'Attribute on an enum' => '#[\Attribute] enum E {}',
    'Attribute on a function' => '#[\Attribute] function f() {}',
    'Attribute on a constant' => '#[\Attribute] const X = 1;',
    'AllowDynamicProperties on a class' => '#[\AllowDynamicProperties] class A {}',
    'AllowDynamicProperties on an abstract one' => '#[\AllowDynamicProperties] abstract class A {}',
    'AllowDynamicProperties on a readonly one' => '#[\AllowDynamicProperties] readonly class A {}',
    'AllowDynamicProperties on an interface' => '#[\AllowDynamicProperties] interface I {}',
    'AllowDynamicProperties on a trait' => '#[\AllowDynamicProperties] trait T {}',
    'AllowDynamicProperties on an enum' => '#[\AllowDynamicProperties] enum E {}',
    'AllowDynamicProperties on a method' => 'class X { #[\AllowDynamicProperties] public function m() {} }',
    'Deprecated on a function' => '#[\Deprecated] function x() {} x();',
    'Deprecated on a class' => '#[\Deprecated] class X {}',
    'Deprecated on an abstract class' => '#[\Deprecated] abstract class X {}',
    'Deprecated on an interface' => '#[\Deprecated] interface I {}',
    'Deprecated on an enum' => '#[\Deprecated] enum E {}',
    'Deprecated on a TRAIT, which php allows' => '#[\Deprecated] trait T {} echo "ok\n";',
    'Deprecated on a class constant' => 'class A { #[\Deprecated] const X = 1; } echo A::X, "\n";',
    'Deprecated on an enum case' => 'enum E { #[\Deprecated] case A; } echo "ok\n";',
    'Deprecated on a property' => 'class X { #[\Deprecated] public $p; }',
    'Deprecated on a parameter' => 'function x(#[\Deprecated] $a) {}',
    'SensitiveParameter on a parameter' => 'function x(#[\SensitiveParameter] $a) {} echo "ok\n";',
    'SensitiveParameter on a promoted one' => 'class C { public function __construct(#[\SensitiveParameter] public $p) {} } echo "ok\n";',
    'SensitiveParameter on a property' => 'class X { #[\SensitiveParameter] public $p; }',
    'SensitiveParameter on a function' => '#[\SensitiveParameter] function f() {}',
    'ReturnTypeWillChange on a method' => 'class X { #[\ReturnTypeWillChange] public function m() {} } echo "ok\n";',
    'ReturnTypeWillChange on a function' => '#[\ReturnTypeWillChange] function f() {}',
    'ReturnTypeWillChange on a property' => 'class X { #[\ReturnTypeWillChange] public $p; }',
    'ReturnTypeWillChange on a class constant' => 'class X { #[\ReturnTypeWillChange] const K = 1; }',
    'ReturnTypeWillChange on a class' => '#[\ReturnTypeWillChange] class X {}',
    'ReturnTypeWillChange on a promoted parameter' => 'class C { public function __construct(#[\ReturnTypeWillChange] public $p) {} }',
    'Override on a function' => '#[\Override] function f() {}',
    'Override on a class' => '#[\Override] class X {}',
    'Override on a class constant' => 'class A { const X = 1; } class B extends A { #[\Override] const X = 2; }',
    'Override on a plain parameter' => 'function f(#[\Override] $a) {}',
    'Override on a promoted one, which is a property' => 'class A { public $p; } class B extends A { public function __construct(#[\Override] public $p) {} } echo "ok\n";',
    'NoDiscard on a class' => '#[\NoDiscard] class X {}',
    'NoDiscard on a property' => 'class X { #[\NoDiscard] public $p; }',
    'NoDiscard on a parameter' => 'function f(#[\NoDiscard] $a) {}',
    'NoDiscard on a class constant' => 'class X { #[\NoDiscard] const K = 1; }',
    'NoDiscard on an enum case' => 'enum E { #[\NoDiscard] case A; }',
    'repeated: Deprecated' => '#[\Deprecated] #[\Deprecated] function f() {}',
    'repeated: Attribute' => '#[\Attribute] #[\Attribute] class A {}',
    'repeated: SensitiveParameter' => 'function f(#[\SensitiveParameter] #[\SensitiveParameter] $a) {}',
    'the WRONG TARGET is reported first' => '#[\Override] #[\Override] function f() {}',
    'a userland attribute is not judged here' => '#[\Attribute(\Attribute::TARGET_CLASS)] class MyA {} '
        . 'class B { #[MyA] public function m() {} } echo "compiled\n";',
    'but is at newInstance()' => '#[\Attribute(\Attribute::TARGET_CLASS)] class MyA {} '
        . 'class B { #[MyA] public function m() {} } '
        . 'try { (new ReflectionMethod("B", "m"))->getAttributes()[0]->newInstance(); } '
        . 'catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }',
    'NoDiscard and repetition is judged there too' => '#[\Attribute(\Attribute::TARGET_CLASS)] class MyA {} '
        . '#[MyA] #[MyA] class B {} '
        . 'try { (new ReflectionClass("B"))->getAttributes()[0]->newInstance(); } '
        . 'catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }',
    'an unknown name is nobody\'s business' => '#[\NoSuchAttrHere] function f() {} echo "compiled\n";',
    'NoDiscard and a namespaced spelling is a DIFFERENT name' => 'namespace N; #[Override] function f() {} echo "compiled\n";',
    'while the qualified one is php\'s' => 'namespace N; #[\Override] function f() {}',
];
foreach ($cases as $label => $code) {
    $f = tempnam(sys_get_temp_dir(), 'phlap');
    file_put_contents($f, "<?php\n" . $code . "\n");
    $out = (string)shell_exec(escapeshellarg(PHP_BINARY) . ' ' . escapeshellarg($f) . ' 2>&1');
    /* a diagnostic names the script with symlinks resolved -- macOS's temp
     * dir sits behind /private -- so both spellings are scrubbed */
    $out = str_replace([realpath($f), $f], 'FILE', $out);
    $out = preg_replace('/^Stack trace:\n(#\d+ .*\n)*/m', '', $out);
    $out = trim(preg_replace('/ in FILE(:| on line )\d+/', '', $out));
    echo '### ', $label, "\n", ($out === '' ? '(accepted)' : $out), "\n";
    unlink($f);
}
--EXPECT--
### Attribute on a class
(accepted)
### Attribute on a final class
(accepted)
### Attribute on an abstract one
PHP Fatal error:  Cannot apply #[\Attribute] to abstract class A
### Attribute on an interface
PHP Fatal error:  Cannot apply #[\Attribute] to interface I
### Attribute on a trait
PHP Fatal error:  Cannot apply #[\Attribute] to trait T
### Attribute on an enum
PHP Fatal error:  Cannot apply #[\Attribute] to enum E
### Attribute on a function
PHP Fatal error:  Attribute "Attribute" cannot target function (allowed targets: class)
### Attribute on a constant
PHP Fatal error:  Attribute "Attribute" cannot target constant (allowed targets: class)
### AllowDynamicProperties on a class
(accepted)
### AllowDynamicProperties on an abstract one
(accepted)
### AllowDynamicProperties on a readonly one
PHP Fatal error:  Cannot apply #[\AllowDynamicProperties] to readonly class A
### AllowDynamicProperties on an interface
PHP Fatal error:  Cannot apply #[\AllowDynamicProperties] to interface I
### AllowDynamicProperties on a trait
PHP Fatal error:  Cannot apply #[\AllowDynamicProperties] to trait T
### AllowDynamicProperties on an enum
PHP Fatal error:  Cannot apply #[\AllowDynamicProperties] to enum E
### AllowDynamicProperties on a method
PHP Fatal error:  Attribute "AllowDynamicProperties" cannot target method (allowed targets: class)
### Deprecated on a function
PHP Deprecated:  Function x() is deprecated
### Deprecated on a class
PHP Fatal error:  Cannot apply #[\Deprecated] to class X
### Deprecated on an abstract class
PHP Fatal error:  Cannot apply #[\Deprecated] to class X
### Deprecated on an interface
PHP Fatal error:  Cannot apply #[\Deprecated] to interface I
### Deprecated on an enum
PHP Fatal error:  Cannot apply #[\Deprecated] to enum E
### Deprecated on a TRAIT, which php allows
ok
### Deprecated on a class constant
PHP Deprecated:  Constant A::X is deprecated
1
### Deprecated on an enum case
ok
### Deprecated on a property
PHP Fatal error:  Attribute "Deprecated" cannot target property (allowed targets: class, function, method, class constant, constant)
### Deprecated on a parameter
PHP Fatal error:  Attribute "Deprecated" cannot target parameter (allowed targets: class, function, method, class constant, constant)
### SensitiveParameter on a parameter
ok
### SensitiveParameter on a promoted one
ok
### SensitiveParameter on a property
PHP Fatal error:  Attribute "SensitiveParameter" cannot target property (allowed targets: parameter)
### SensitiveParameter on a function
PHP Fatal error:  Attribute "SensitiveParameter" cannot target function (allowed targets: parameter)
### ReturnTypeWillChange on a method
ok
### ReturnTypeWillChange on a function
PHP Fatal error:  Attribute "ReturnTypeWillChange" cannot target function (allowed targets: method)
### ReturnTypeWillChange on a property
PHP Fatal error:  Attribute "ReturnTypeWillChange" cannot target property (allowed targets: method)
### ReturnTypeWillChange on a class constant
PHP Fatal error:  Attribute "ReturnTypeWillChange" cannot target class constant (allowed targets: method)
### ReturnTypeWillChange on a class
PHP Fatal error:  Attribute "ReturnTypeWillChange" cannot target class (allowed targets: method)
### ReturnTypeWillChange on a promoted parameter
PHP Fatal error:  Attribute "ReturnTypeWillChange" cannot target parameter (allowed targets: method)
### Override on a function
PHP Fatal error:  Attribute "Override" cannot target function (allowed targets: method, property)
### Override on a class
PHP Fatal error:  Attribute "Override" cannot target class (allowed targets: method, property)
### Override on a class constant
PHP Fatal error:  Attribute "Override" cannot target class constant (allowed targets: method, property)
### Override on a plain parameter
PHP Fatal error:  Attribute "Override" cannot target parameter (allowed targets: method, property)
### Override on a promoted one, which is a property
ok
### NoDiscard on a class
PHP Fatal error:  Attribute "NoDiscard" cannot target class (allowed targets: function, method)
### NoDiscard on a property
PHP Fatal error:  Attribute "NoDiscard" cannot target property (allowed targets: function, method)
### NoDiscard on a parameter
PHP Fatal error:  Attribute "NoDiscard" cannot target parameter (allowed targets: function, method)
### NoDiscard on a class constant
PHP Fatal error:  Attribute "NoDiscard" cannot target class constant (allowed targets: function, method)
### NoDiscard on an enum case
PHP Fatal error:  Attribute "NoDiscard" cannot target class constant (allowed targets: function, method)
### repeated: Deprecated
PHP Fatal error:  Attribute "Deprecated" must not be repeated
### repeated: Attribute
PHP Fatal error:  Attribute "Attribute" must not be repeated
### repeated: SensitiveParameter
PHP Fatal error:  Attribute "SensitiveParameter" must not be repeated
### the WRONG TARGET is reported first
PHP Fatal error:  Attribute "Override" cannot target function (allowed targets: method, property)
### a userland attribute is not judged here
compiled
### but is at newInstance()
Error: Attribute "MyA" cannot target method (allowed targets: class)
### NoDiscard and repetition is judged there too
Error: Attribute "MyA" must not be repeated
### an unknown name is nobody's business
compiled
### NoDiscard and a namespaced spelling is a DIFFERENT name
compiled
### while the qualified one is php's
PHP Fatal error:  Attribute "Override" cannot target function (allowed targets: method, property)
