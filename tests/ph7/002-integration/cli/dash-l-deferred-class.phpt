--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
`-l` parses a class body whose base it cannot see, and binds nothing
--DESCRIPTION--
A syntax check PARSES; it does not BIND. php's lint compiles every class body and
reports a syntax error in it whatever the class extends, while it reports NOTHING
that needs the parent resolved -- a missing base, an `#[\Override]` with no visible
parent method, an unimplemented abstract inherited through a trait it cannot see, a
`parent` return type, or a redeclared class name (php takes a class name at the
DECLARE_CLASS opcode, which lint never runs; a redeclared FUNCTION it does report).

PHL DEFERRED such a declaration -- captured the body as raw text to re-compile at
its execution point -- so under `-l`, where nothing autoloads, the body was never
parsed at all and `$x = ;` inside it linted clean. In a PSR-4 tree the base is
almost never in the same file, so that was most of the corpus.

Only the VERDICT is compared per case: the two engines word several of these
refusals differently, and what this test is about is which ones there are.
--SKIPIF--
<?php if (PHP_OS == 'WINNT') { echo "skip POSIX-only behavior; not applicable on Windows"; } ?>
--FILE--
<?php
$phl = getenv('PHPT_TARGET_EXECUTABLE');
$f   = sys_get_temp_dir() . '/phl_lintdefer_' . getmypid() . '.php';

function dldLint($phl, $f, $label, $src) {
    file_put_contents($f, "<?php\n" . $src . "\n");
    $fp = popen("\"$phl\" -l \"$f\" 2>&1", 'r');
    $o = ''; while (!feof($fp)) { $o .= fgets($fp); } $rc = pclose($fp);
    printf("%-14s %s\n", $label, $rc === 0 ? 'clean' : 'refused');
}

// The body IS parsed, whatever stands between the class and its base.
dldLint($phl, $f, 'iface-missing', 'class C implements NoSuchIface { public function m() { $x = ; } }');
dldLint($phl, $f, 'base-missing',  'class D extends NoSuchBase { public function m() { $x = ; } }');
dldLint($phl, $f, 'trait-missing', 'class E { use NoSuchTrait; public function m() { $x = ; } }');
dldLint($phl, $f, 'iface-extends', 'interface I extends NoSuchIface { function m(): ; }');
dldLint($phl, $f, 'conditional',   'if (false) { class F { public function m() { $x = ; } } }');
dldLint($phl, $f, 'in-function',   'function zf() { class G { public function m() { $x = ; } } }');
dldLint($phl, $f, 'anonymous',     '$o = new class extends NoSuchBase { public function m() { $x = ; } };');

// And nothing that needs the base RESOLVED is reported.
dldLint($phl, $f, 'no-base',       'class H extends NoSuchBase {}');
dldLint($phl, $f, 'override',      'class J extends NoSuchBase { #[\Override] public function z() {} }');
dldLint($phl, $f, 'abstract-trait','class K implements ArrayAccess { use NoSuchTrait; }');
dldLint($phl, $f, 'parent-type',   'class L extends NoSuchBase { public function c(): parent {} }');
dldLint($phl, $f, 'class-twice',   'class M {} class M {}');
dldLint($phl, $f, 'iface-twice',   'interface N {} interface N {}');
dldLint($phl, $f, 'internal-name', 'class DateTime {}');

// What php DOES report at lint time still is reported.
dldLint($phl, $f, 'func-twice',    'function zg() {} function zg() {}');
dldLint($phl, $f, 'abstract-left', 'abstract class P { abstract function m(); } class Q extends P {}');
dldLint($phl, $f, 'method-twice',  'class R { function m() {} function m() {} }');

// The line and wording of the ordinary case, which both engines share.
file_put_contents($f, "<?php\n\nclass S extends NoSuchBase\n{\n    public function m() { \$x = ; }\n}\n");
$fp = popen("\"$phl\" -l \"$f\" 2>&1", 'r');
$o = ''; while (!feof($fp)) { $o .= fgets($fp); } pclose($fp);
/* a diagnostic may name the file with symlinks resolved (macOS's /var is /private/var) */
echo str_replace([realpath($f) ?: $f, $f, "\r\n"], ['F', 'F', "\n"], $o);
@unlink($f);
?>
--EXPECT--
iface-missing  refused
base-missing   refused
trait-missing  refused
iface-extends  refused
conditional    refused
in-function    refused
anonymous      refused
no-base        clean
override       clean
abstract-trait clean
parent-type    clean
class-twice    clean
iface-twice    clean
internal-name  clean
func-twice     refused
abstract-left  refused
method-twice   refused
PHP Parse error:  syntax error, unexpected token ";" in F on line 5
Errors parsing F
