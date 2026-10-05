--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A refused attribute on a hooked property blames the end of its last hook
--DESCRIPTION--
php judges a property's attributes last, after compiling its hooks, and each
hook is a function whose implicit return is placed on its final line -- so the
refusal names the line that ends the last hook: its body's `}`, or the `;` of
an arrow or abstract hook, never the declaration's line. Every other refusal of
the declaration therefore wins over the attribute: a readonly or abstract
property that cannot be one, and, in a class with no parent, a default value on
a virtual hooked property (with a parent that check waits for the class link,
after the attributes, and blames the class's line). An unhooked property still
blames its declaration.
--FILE--
<?php
$cases = [
    'arrow get' => "class C {\n#[\\NoDiscard]\npublic int \$x\n{\nget => 1;\n}\n}",
    'block get' => "class C {\n#[\\NoDiscard]\npublic int\n\$x {\nget {\nreturn 1;\n}\n}\n}",
    'arrow split over lines' => "class C {\n#[\\NoDiscard]\npublic \$x {\nget\n=>\n1\n;\n}\n}",
    'two hooks, block last' => "class C {\n#[\\NoDiscard]\npublic int \$x {\nget => 1;\nset {\n\$this->x = 2;\n}\n}\n}",
    'two hooks, arrow last' => "class C {\n#[\\NoDiscard]\npublic int \$x { set(int \$v) { \$this->x = 1;\n}\nget\n=> 1; }\n}",
    'set with a parameter list' => "class C {\n#[\\NoDiscard]\npublic int \$x {\nset\n(\nint \$v\n)\n{\n}\n}\n}",
    'abstract hooks' => "abstract class C {\n#[\\NoDiscard]\nabstract public int \$x {\nget;\nset\n;\n}\n}",
    'interface hook' => "interface I {\n#[\\NoDiscard]\npublic int \$x {\nget;\n}\n}",
    'repeated' => "class C {\n#[\\Override] #[\\Override]\npublic int \$x {\nget => 1;\n}\n}",
    'unhooked property after a hooked one' => "class C {\npublic int \$x {\nget => 1;\n}\n#[\\NoDiscard]\npublic int\n\$y = 2;\n}",
    'readonly hooked' => "class C {\n#[\\NoDiscard]\npublic readonly int \$x { get => 1; }\n}",
    'abstract unhooked' => "abstract class C {\n#[\\NoDiscard]\nabstract public int \$x;\n}",
    'virtual default, no parent' => "class C {\n#[\\NoDiscard]\npublic int \$x = 1 {\nget => 1;\n}\n}",
    'virtual default, with a parent' => "class P {}\nclass C extends P {\n#[\\NoDiscard]\npublic int \$x = 1 {\nget => 1;\n}\n}",
    'virtual default alone, no parent' => "class C {\npublic int \$x = 1 {\nget => 1;\n}\n}",
    'virtual default alone, with a parent' => "class P {}\nclass C\nextends P {\npublic int \$x = 1 {\nget => 1;\n}\n}",
];
foreach ($cases as $label => $code) {
    $f = tempnam(sys_get_temp_dir(), 'phlah');
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
### arrow get
PHP Fatal error:  Attribute "NoDiscard" cannot target property (allowed targets: function, method) in FILE on line 6
### block get
PHP Fatal error:  Attribute "NoDiscard" cannot target property (allowed targets: function, method) in FILE on line 8
### arrow split over lines
PHP Fatal error:  Attribute "NoDiscard" cannot target property (allowed targets: function, method) in FILE on line 8
### two hooks, block last
PHP Fatal error:  Attribute "NoDiscard" cannot target property (allowed targets: function, method) in FILE on line 8
### two hooks, arrow last
PHP Fatal error:  Attribute "NoDiscard" cannot target property (allowed targets: function, method) in FILE on line 7
### set with a parameter list
PHP Fatal error:  Attribute "NoDiscard" cannot target property (allowed targets: function, method) in FILE on line 10
### abstract hooks
PHP Fatal error:  Attribute "NoDiscard" cannot target property (allowed targets: function, method) in FILE on line 7
### interface hook
PHP Fatal error:  Attribute "NoDiscard" cannot target property (allowed targets: function, method) in FILE on line 5
### repeated
PHP Fatal error:  Attribute "Override" must not be repeated in FILE on line 5
### unhooked property after a hooked one
PHP Fatal error:  Attribute "NoDiscard" cannot target property (allowed targets: function, method) in FILE on line 7
### readonly hooked
PHP Fatal error:  Hooked properties cannot be readonly in FILE on line 4
### abstract unhooked
PHP Fatal error:  Only hooked properties may be declared abstract in FILE on line 4
### virtual default, no parent
PHP Fatal error:  Cannot specify default value for virtual hooked property C::$x in FILE on line 5
### virtual default, with a parent
PHP Fatal error:  Attribute "NoDiscard" cannot target property (allowed targets: function, method) in FILE on line 6
### virtual default alone, no parent
PHP Fatal error:  Cannot specify default value for virtual hooked property C::$x in FILE on line 4
### virtual default alone, with a parent
PHP Fatal error:  Cannot specify default value for virtual hooked property C::$x in FILE on line 3
