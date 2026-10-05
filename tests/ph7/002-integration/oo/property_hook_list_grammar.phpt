--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A property hook list follows php's grammar and its per-hook rules
--DESCRIPTION--
A hook is `[final] [&] name [(params)]` and then `;`, a `{...}` body or
`=> expr;`. Every other member modifier is refused word by word ("Cannot use
the public modifier on a property hook", at the token that ends the run), a
stray `;` between hooks is a parse error, and the parser's refusals outrank the
per-hook rules wherever they sit. Those rules run in php's order at the hook's
name: static property, final+private, abstract body/final, missing body, the
hook's name, a get parameter list, the set parameter's shape, a redeclared
hook. PHL refused `final` outright, skipped a stray `;`, ran an abstract
property's concrete hook as a refusal, and compiled most of the rest.
Each snippet runs in a process of its own through PHP_BINARY.
--FILE--
<?php
$dir = sys_get_temp_dir() . '/phlt-hookgram-' . getmypid();
@mkdir($dir);

$cases = [
    'final get'            => "class A { public int \$x {\n final get => 1;\n} }",
    'final set override'   => "class A { public int \$x {\n final get => 1;\n set { \$this->x = \$value; }\n} }\nclass B extends A { public int \$x {\n set { \$this->x = \$value * 2; }\n} }",
    'final get override'   => "class A { public int \$x {\n final get => 1;\n} }\nclass B extends A { public int \$x {\n get => 2;\n} }",
    'public get'           => "class A { public int \$x {\n public\n static get => 1;\n} }",
    'final final'          => "class A { public int \$x {\n final final get => 1;\n} }",
    'private(set) get'     => "class A { public int \$x {\n private(set) get => 1;\n} }",
    'final private'        => "class A { private int \$x {\n final get => 1;\n} }",
    'abstract final'       => "abstract class A { abstract public int \$x {\n final get;\n} }",
    'interface final'      => "interface I { public int \$x {\n final get;\n} }",
    'abstract with body'   => "abstract class A { abstract public int \$x {\n get => 1;\n set;\n} }",
    'empty list'           => "class A { public int \$x {\n} }",
    'static property'      => "class A { public static int \$x {\n get => 1;\n} }",
    'unknown name'         => "class A { public int \$x {\n get => 1;\n foo => 2;\n} }",
    'unknown bodyless'     => "class A { public int \$x {\n foo;\n} }",
    'stray semicolon'      => "class A { public int \$x {\n foo => 1;\n ;\n} }",
    'no identifier'        => "class A { public int \$x {\n 1;\n} }",
    'no body'              => "class A { public int \$x {\n get\n} }",
    'junk after params'    => "class A { public int \$x {\n set(int \$v) 1;\n} }",
    'arrow without ;'      => "class A { public int \$x {\n get => 1\n} }",
    'get parameters'       => "class A { public int \$x {\n get() => 1;\n} }",
    'set two parameters'   => "class A { public int \$x {\n Set(\$a, \$b) {}\n} }",
    'set by reference'     => "class A { public int \$x {\n set(int &\$v) {}\n} }",
    'set variadic'         => "class A { public int \$x {\n set(int ...\$v) {}\n} }",
    'set default'          => "class A { public int \$x {\n set(int \$v = 1) {}\n} }",
    'set untyped'          => "class A { public int \$x {\n set(\$v) {}\n} }",
    'set typed, untyped'   => "class A { public \$x {\n set(int \$v) {}\n} }",
    'redeclared'           => "class A { public int \$x {\n get => 1;\n GET\n {\n return 2;\n }\n} }",
];

$n = 0;
foreach ($cases as $label => $src) {
    $file = $dir . '/case' . $n++ . '.php';
    file_put_contents($file, "<?php\n" . $src . "\n"
        . "\$r = new ReflectionProperty('A', 'x');\n"
        . "\$g = \$r->getHook(PropertyHookType::Get);\n"
        . "echo 'ok', \$g ? ' get final=' . var_export(\$g->isFinal(), true) : '', \"\\n\";\n");
    $out = [];
    exec(escapeshellarg(PHP_BINARY) . ' -d display_errors=1 -d log_errors=0 -d html_errors=0 '
        . escapeshellarg($file) . ' 2>&1', $out);
    $said = [];
    foreach ($out as $l) {
        if ($l === '' || str_starts_with($l, 'Stack trace') || str_starts_with($l, '#')) {
            continue;
        }
        $said[] = preg_replace('/ in \S+ on line (\d+)$/', ' (line $1)', trim($l));
    }
    echo $label, ': ', implode(' | ', $said), "\n";
    @unlink($file);
}
@rmdir($dir);
?>
--EXPECT--
final get: ok get final=true
final set override: ok get final=true
final get override: Fatal error: Cannot override final property hook A::$x::get() (line 5)
public get: Fatal error: Cannot use the public modifier on a property hook (line 4)
final final: Fatal error: Multiple final modifiers are not allowed (line 3)
private(set) get: Fatal error: Cannot use the private(set) modifier on a property hook (line 3)
final private: Fatal error: Property hook cannot be both final and private (line 3)
abstract final: Fatal error: Property hook cannot be both abstract and final (line 3)
interface final: Fatal error: Property hook cannot be both abstract and final (line 3)
abstract with body: ok get final=false
empty list: Fatal error: Property hook list must not be empty (line 2)
static property: Fatal error: Cannot declare hooks for static property (line 3)
unknown name: Fatal error: Unknown hook "foo" for property A::$x, expected "get" or "set" (line 4)
unknown bodyless: Fatal error: Non-abstract property hook must have a body (line 3)
stray semicolon: Parse error: syntax error, unexpected token ";", expecting identifier (line 4)
no identifier: Parse error: syntax error, unexpected integer "1", expecting identifier (line 3)
no body: Parse error: syntax error, unexpected token "}", expecting "=>" or ";" or "{" (line 4)
junk after params: Parse error: syntax error, unexpected integer "1", expecting "=>" or ";" or "{" (line 3)
arrow without ;: Parse error: syntax error, unexpected token "}" (line 4)
get parameters: Fatal error: get hook of property A::$x must not have a parameter list (line 3)
set two parameters: Fatal error: Set hook of property A::$x must accept exactly one parameters (line 3)
set by reference: Fatal error: Parameter $v of set hook A::$x must not be pass-by-reference (line 3)
set variadic: Fatal error: Parameter $v of set hook A::$x must not be variadic (line 3)
set default: Fatal error: Parameter $v of set hook A::$x must not have a default value (line 3)
set untyped: Fatal error: Type of parameter $v of hook A::$x::set must be compatible with property type (line 3)
set typed, untyped: Fatal error: Type of parameter $v of hook A::$x::set must be compatible with property type (line 3)
redeclared: Fatal error: Cannot redeclare property hook "GET" (line 7)
