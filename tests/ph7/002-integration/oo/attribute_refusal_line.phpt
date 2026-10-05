--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A refused attribute blames its declaration's line, not its own
--DESCRIPTION--
php validates a declaration's attributes while it compiles the declaration's
AST node, so the line it reports is the node's: the declaring keyword for a
function, closure or class-like (modifiers in front move nothing), the type --
else the name -- for a property or a parameter, and the first name for a
constant, a class-constant group or an enum case. A const statement's other
diagnostics share that line, and a list carrying attributes is refused before
any of them is judged. A `static` closure's line is its `function` keyword's,
which getStartLine() and the closure's name read too.
--FILE--
<?php
$cases = [
    'function' => "#[\\Attribute]\n\nfunction\n&\nf() {}",
    'final class' => "#[\\Override]\nfinal\nclass\nC {}",
    'interface' => "#[\\Override]\n\ninterface\nI {}",
    'trait' => "#[\\Override]\n\ntrait\nT {}",
    'enum' => "#[\\Override]\n\nenum\nE {}",
    'class validator' => "#[\\Deprecated]\nfinal\nclass\nC {}",
    'repeated' => "#[\\Deprecated]\n#[\\Deprecated]\n\nfunction f() {}",
    'method' => "class C {\n#[\\Attribute]\npublic\nstatic\nfunction\nm() {}\n}",
    'typed property' => "class C {\n#[\\Attribute]\npublic\nint\n\$x,\n\$y;\n}",
    'untyped property' => "class C {\n#[\\Attribute]\npublic\n\n\$x;\n}",
    'class constant' => "class C {\n#[\\Attribute]\npublic\nconst\nint\nK = 1,\nL = 2;\n}",
    'enum case' => "enum E {\n#[\\Attribute]\ncase\nA;\n}",
    'typed parameter' => "function f(\n#[\\Attribute]\nint\n\$x\n) {}",
    'untyped parameter' => "function f(\n#[\\Attribute]\n\n\$x\n) {}",
    'promoted parameter' => "class C {\npublic function __construct(\n#[\\Attribute]\npublic\nint\n\$x\n) {}\n}",
    'closure' => "\$f =\n#[\\Attribute]\nfunction\n() {};",
    'static closure' => "\$f =\n#[\\Attribute]\nstatic\nfunction\n() {};",
    'arrow function' => "\$f =\n#[\\Attribute]\nfn\n() => 1;",
    'anonymous class' => "\$o = new\n#[\\Override]\nclass\n{};",
    'anonymous class validator' => "\$o = new\n#[\\Deprecated]\nclass\n{};",
    'static closure start line' => "\$f =\nstatic\nfunction\n() { return __FUNCTION__; };\necho (new ReflectionFunction(\$f))->getStartLine(), ' ', \$f(), \"\\n\";",
    'static arrow function start line' => "\$f =\nstatic\nfn\n() => 1;\necho (new ReflectionFunction(\$f))->getStartLine(), \"\\n\";",
    'constant' => "#[\\Attribute]\nconst\nX = 1;",
    'constant list' => "#[\\Attribute]\nconst\nA = 1,\nB = 2;",
    'constant list, invalid value' => "const\nA = 1,\nB = strlen('x');",
    'constant redeclared' => "const\nA = 1;\nconst\nB = 2,\nA = 3;",
];
foreach ($cases as $label => $code) {
    $f = tempnam(sys_get_temp_dir(), 'phlal');
    file_put_contents($f, "<?php\n" . $code . "\n");
    $out = (string)shell_exec(escapeshellarg(PHP_BINARY) . ' ' . escapeshellarg($f) . ' 2>&1');
    /* a diagnostic names the script with symlinks resolved -- macOS's temp
     * dir sits behind /private -- so both spellings are scrubbed */
    $out = str_replace([realpath($f), $f], 'FILE', $out);
    $out = trim(preg_replace('/^Stack trace:\n(#\d+ .*\n)*/m', '', $out));
    echo '### ', $label, "\n", ($out === '' ? '(accepted)' : $out), "\n";
    unlink($f);
}
--EXPECT--
### function
PHP Fatal error:  Attribute "Attribute" cannot target function (allowed targets: class) in FILE on line 4
### final class
PHP Fatal error:  Attribute "Override" cannot target class (allowed targets: method, property) in FILE on line 4
### interface
PHP Fatal error:  Attribute "Override" cannot target class (allowed targets: method, property) in FILE on line 4
### trait
PHP Fatal error:  Attribute "Override" cannot target class (allowed targets: method, property) in FILE on line 4
### enum
PHP Fatal error:  Attribute "Override" cannot target class (allowed targets: method, property) in FILE on line 4
### class validator
PHP Fatal error:  Cannot apply #[\Deprecated] to class C in FILE on line 4
### repeated
PHP Fatal error:  Attribute "Deprecated" must not be repeated in FILE on line 5
### method
PHP Fatal error:  Attribute "Attribute" cannot target method (allowed targets: class) in FILE on line 6
### typed property
PHP Fatal error:  Attribute "Attribute" cannot target property (allowed targets: class) in FILE on line 5
### untyped property
PHP Fatal error:  Attribute "Attribute" cannot target property (allowed targets: class) in FILE on line 6
### class constant
PHP Fatal error:  Attribute "Attribute" cannot target class constant (allowed targets: class) in FILE on line 7
### enum case
PHP Fatal error:  Attribute "Attribute" cannot target class constant (allowed targets: class) in FILE on line 5
### typed parameter
PHP Fatal error:  Attribute "Attribute" cannot target parameter (allowed targets: class) in FILE on line 4
### untyped parameter
PHP Fatal error:  Attribute "Attribute" cannot target parameter (allowed targets: class) in FILE on line 5
### promoted parameter
PHP Fatal error:  Attribute "Attribute" cannot target parameter (allowed targets: class) in FILE on line 6
### closure
PHP Fatal error:  Attribute "Attribute" cannot target function (allowed targets: class) in FILE on line 4
### static closure
PHP Fatal error:  Attribute "Attribute" cannot target function (allowed targets: class) in FILE on line 5
### arrow function
PHP Fatal error:  Attribute "Attribute" cannot target function (allowed targets: class) in FILE on line 4
### anonymous class
PHP Fatal error:  Attribute "Override" cannot target class (allowed targets: method, property) in FILE on line 4
### anonymous class validator
PHP Fatal error:  Cannot apply #[\Deprecated] to class class@anonymous in FILE on line 4
### static closure start line
4 {closure:FILE:4}
### static arrow function start line
4
### constant
PHP Fatal error:  Attribute "Attribute" cannot target constant (allowed targets: class) in FILE on line 4
### constant list
PHP Fatal error:  Cannot apply attributes to multiple constants at once in FILE on line 4
### constant list, invalid value
PHP Fatal error:  Constant expression contains invalid operations in FILE on line 3
### constant redeclared
PHP Warning:  Constant A already defined, this will be an error in PHP 9 in FILE on line 5
