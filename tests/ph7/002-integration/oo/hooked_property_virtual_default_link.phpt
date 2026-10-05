--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A default on a hooked property in a class with a parent is judged at the class link
--DESCRIPTION--
A hooked property whose own bodies never touch `$this->NAME` is virtual, and a
virtual property refuses a default -- but in a class with a parent that is
only known once the class links, because redeclaring a BACKED parent property
makes the child backed too. Over a backed parent (or grandparent) the default
is accepted; over a virtual one, or with no parent property of that name, the
refusal blames the class's own line. A `parent::$x::get()` call is not a
backing reference.
--FILE--
<?php
$cases = [
    'over a backed parent' => "class P { public int \$x = 1; }\nclass C extends P {\npublic int \$x = 2 {\nget => 7;\n}\n}\nvar_dump((new C)->x);",
    'over a backed grandparent' => "class G { public int \$x = 1; }\nclass P extends G {}\nclass C extends P {\npublic int \$x = 2 { get => 8; }\n}\nvar_dump((new C)->x);",
    'over a hooked backed parent' => "class P { public int \$x = 1 { get => \$this->x; } }\nclass C extends P {\npublic int \$x = 2 { get => parent::\$x::get() + 1; }\n}\nvar_dump((new C)->x);",
    'over a virtual parent' => "class P { public int \$x { get => 1; } }\nclass C\nextends P {\npublic int \$x = 2 { get => parent::\$x::get() + 1; }\n}",
    'no parent property' => "class P { public int \$y = 1; }\nclass C extends P {\npublic int \$x = 2 {\nget => 4;\n}\n}",
    'untyped over a virtual parent' => "class P { public \$x { get => 1; } }\nclass C extends P { public \$x = null { get => 2; } }",
];
foreach ($cases as $label => $code) {
    $f = tempnam(sys_get_temp_dir(), 'phlvd');
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
### over a backed parent
int(7)
### over a backed grandparent
int(8)
### over a hooked backed parent
int(3)
### over a virtual parent
PHP Fatal error:  Cannot specify default value for virtual hooked property C::$x in FILE on line 3
### no parent property
PHP Fatal error:  Cannot specify default value for virtual hooked property C::$x in FILE on line 3
### untyped over a virtual parent
PHP Fatal error:  Cannot specify default value for virtual hooked property C::$x in FILE on line 3
