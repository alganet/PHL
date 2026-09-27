--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
#[\Override] is a claim the engine checks where the member is written
--DESCRIPTION--
php 8.3's `#[\Override]` (widened to properties in 8.5) says the member already
exists above, and the engine verifies it at class-link time: a typo, a renamed
parent method or a base class that dropped one becomes a fatal at the
declaration instead of a method nobody ever calls. The attribute class did not
exist here and nothing checked the claim, so every one of these compiled.

The rules are the INHERITANCE rules, not a name search: a private parent member
is not inherited and so is not overridable; the CONSTRUCTOR is exempt from php's
signature check unless it is abstract (or an interface's), and the claim follows
that exemption; a method matches case-insensitively and a property
case-sensitively; a trait used by this very class is the using class's own
member, not a parent's.
--FILE--
<?php
$cases = [
    'parent has the method' =>
        'class A { public function m() {} } class B extends A { #[\Override] public function m() {} }',
    'no parent method' =>
        'class A {} class B extends A { #[\Override] public function m() {} }',
    'no base class at all' =>
        'class B { #[\Override] public function m() {} }',
    'an interface declares it' =>
        'interface I { public function m(); } class B implements I { #[\Override] public function m() {} }',
    'an interface two levels up' =>
        'interface I { public function m(); } interface J extends I {} class B implements J { #[\Override] public function m() {} }',
    'an ancestor implements it' =>
        'interface I { public function m(); } abstract class A implements I {} class B extends A { #[\Override] public function m() {} }',
    'the grandparent has it' =>
        'class A { public function m() {} } class M extends A {} class B extends M { #[\Override] public function m() {} }',
    'a private parent method is not inherited' =>
        'class A { private function m() {} } class B extends A { #[\Override] public function m() {} }',
    'static counts' =>
        'class A { public static function m() {} } class B extends A { #[\Override] public static function m() {} }',
    'static with nothing above' =>
        'class A {} class B extends A { #[\Override] public static function m() {} }',
    'method names fold case' =>
        'class A { public function MyM() {} } class B extends A { #[\Override] public function mym() {} }',
    'a concrete parent constructor does not count' =>
        'class A { public function __construct() {} } class B extends A { #[\Override] public function __construct() {} }',
    'an abstract one does' =>
        'abstract class A { abstract public function __construct(); } class B extends A { #[\Override] public function __construct() {} }',
    'and so does an interface one' =>
        'interface I { public function __construct(); } class A implements I { public function __construct() {} } class B extends A { #[\Override] public function __construct() {} }',
    'a trait used HERE is not a parent' =>
        'trait T { public function m() {} } class B { use T; #[\Override] public function m() {} }',
    'a trait used by the PARENT is' =>
        'trait T { public function m() {} } class A { use T; } class B extends A { #[\Override] public function m() {} }',
    'the claim travels with the trait' =>
        'trait T { #[\Override] public function m() {} } class B { use T; }',
    'an interface may claim it too' =>
        'interface I { public function m(); } interface J extends I { #[\Override] public function m(); }',
    'an interface with no parent may not' =>
        'interface J { #[\Override] public function m(); }',
    'an enum method against its interface' =>
        'interface I { public function m(); } enum E implements I { case A; #[\Override] public function m() {} }',
    'an enum method against nothing' =>
        'enum E { case A; #[\Override] public function m() {} }',
    'the child of a checked class is not re-checked' =>
        'class A { public function m() {} } class B extends A { #[\Override] public function m() {} } class C extends B { public function m() {} }',
    'a property the parent declares' =>
        'class A { public $p; } class B extends A { #[\Override] public $p; }',
    'a property nothing declares' =>
        'class A {} class B extends A { #[\Override] public $p; }',
    'a private parent property' =>
        'class A { private $p; } class B extends A { #[\Override] public $p; }',
    'a protected one widened' =>
        'class A { protected $p; } class B extends A { #[\Override] public $p; }',
    'property names do NOT fold case' =>
        'class A { public $P; } class B extends A { #[\Override] public $p; }',
    'a static property' =>
        'class A { public static $p; } class B extends A { #[\Override] public static $p; }',
    'a hooked property over a plain one' =>
        'class A { public $p; } class B extends A { #[\Override] public $p { get => 1; } }',
    'a hooked property over nothing' =>
        'class B { #[\Override] public $p { get => 1; } }',
    'a promoted property the parent has' =>
        'class A { public $p; } class B extends A { public function __construct(#[\Override] public $p) {} }',
    'a promoted property nothing has' =>
        'class A {} class B extends A { public function __construct(#[\Override] public $p) {} }',
    'the METHOD is reported before the property' =>
        'class A {} class B extends A { #[\Override] public $p; #[\Override] public function m() {} }',
    'and the FIRST method of two' =>
        'class A {} class B extends A { #[\Override] public function m() {} #[\Override] public function n() {} }',
    'an earlier fatal wins' =>
        'abstract class A { abstract function z(); } class B extends A { #[\Override] public function m() {} }',
    'the attribute is still an ordinary reflectable one' =>
        'class A { public function m() {} } class B extends A { #[\Override] public function m() {} }'
        . ' $a = (new ReflectionMethod("B", "m"))->getAttributes()[0];'
        . ' echo $a->getName(), "/", get_class($a->newInstance()), "\n";',
];
/* Each case is its own COMPILE, so it needs its own process: a #[\Override]
 * failure is a fatal at the declaration and the first one would end the run. */
foreach ($cases as $label => $code) {
    $f = tempnam(sys_get_temp_dir(), 'phlov');
    file_put_contents($f, "<?php\n" . $code . "\n");
    $out = (string)shell_exec(escapeshellarg(PHP_BINARY) . ' ' . escapeshellarg($f) . ' 2>&1');
    /* a diagnostic names the script with symlinks resolved -- macOS's temp
     * dir sits behind /private -- so both spellings are scrubbed */
    $out = str_replace([realpath($f), $f], 'FILE', $out);
    $out = preg_replace('/^Stack trace:\n#0 \{main\}\n?/m', '', $out);
    $out = trim(preg_replace('/ in FILE on line \d+/', '', $out));
    echo '### ', $label, "\n", ($out === '' ? '(accepted)' : $out), "\n";
    unlink($f);
}
--EXPECT--
### parent has the method
(accepted)
### no parent method
PHP Fatal error:  B::m() has #[\Override] attribute, but no matching parent method exists
### no base class at all
PHP Fatal error:  B::m() has #[\Override] attribute, but no matching parent method exists
### an interface declares it
(accepted)
### an interface two levels up
(accepted)
### an ancestor implements it
(accepted)
### the grandparent has it
(accepted)
### a private parent method is not inherited
PHP Fatal error:  B::m() has #[\Override] attribute, but no matching parent method exists
### static counts
(accepted)
### static with nothing above
PHP Fatal error:  B::m() has #[\Override] attribute, but no matching parent method exists
### method names fold case
(accepted)
### a concrete parent constructor does not count
PHP Fatal error:  B::__construct() has #[\Override] attribute, but no matching parent method exists
### an abstract one does
(accepted)
### and so does an interface one
(accepted)
### a trait used HERE is not a parent
PHP Fatal error:  B::m() has #[\Override] attribute, but no matching parent method exists
### a trait used by the PARENT is
(accepted)
### the claim travels with the trait
PHP Fatal error:  B::m() has #[\Override] attribute, but no matching parent method exists
### an interface may claim it too
(accepted)
### an interface with no parent may not
PHP Fatal error:  J::m() has #[\Override] attribute, but no matching parent method exists
### an enum method against its interface
(accepted)
### an enum method against nothing
PHP Fatal error:  E::m() has #[\Override] attribute, but no matching parent method exists
### the child of a checked class is not re-checked
(accepted)
### a property the parent declares
(accepted)
### a property nothing declares
PHP Fatal error:  B::$p has #[\Override] attribute, but no matching parent property exists
### a private parent property
PHP Fatal error:  B::$p has #[\Override] attribute, but no matching parent property exists
### a protected one widened
(accepted)
### property names do NOT fold case
PHP Fatal error:  B::$p has #[\Override] attribute, but no matching parent property exists
### a static property
(accepted)
### a hooked property over a plain one
(accepted)
### a hooked property over nothing
PHP Fatal error:  B::$p has #[\Override] attribute, but no matching parent property exists
### a promoted property the parent has
(accepted)
### a promoted property nothing has
PHP Fatal error:  B::$p has #[\Override] attribute, but no matching parent property exists
### the METHOD is reported before the property
PHP Fatal error:  B::m() has #[\Override] attribute, but no matching parent method exists
### and the FIRST method of two
PHP Fatal error:  B::m() has #[\Override] attribute, but no matching parent method exists
### an earlier fatal wins
PHP Fatal error:  Class B contains 1 abstract method and must therefore be declared abstract or implement the remaining method (A::z)
### the attribute is still an ordinary reflectable one
Override/Override
