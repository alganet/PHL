--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ATTR_STATEMENT_CLASS: the constructor php calls, and the one it refuses
--DESCRIPTION--
php builds the statement OBJECT itself and then calls the class's own
constructor with the arguments the attribute was given. That constructor is the
engine's to call, not a script's, so php refuses a class whose constructor is
PUBLIC — `User-supplied statement class cannot have a public constructor` — and
runs a protected or private one without a scope check, which is what the
documented subclass declares. Arguments with no constructor to take them are
refused where the statement is BUILT, not where the attribute is set:
`User-supplied statement does not accept constructor arguments`.

getAttribute() answers the pair as it was set — the class alone until arguments
ride beside it. And the class is stored BEFORE the arguments are judged, so a
bad `constructor_args` refuses with the new class already standing and the old
arguments already dropped.

The same engine dispatch answers FETCH_CLASS: php fills and constructs an
object of a class whose constructor is private there too.
--FILE--
<?php
class ScmPublic extends PDOStatement { public function __construct($z = null) {} }
class ScmProt extends PDOStatement { public $z; protected function __construct($z = null) { $this->z = $z; } }
class ScmPriv extends PDOStatement { public $z; private function __construct($z = null) { $this->z = $z; } }
class ScmPlain extends PDOStatement { public $z; }
class ScmNotAStatement {}
class ScmFetchPriv { public $a; public $z; private function __construct($z = null) { $this->z = $z; } }

$scmDb = new PDO('sqlite::memory:');
$scmDb->exec('CREATE TABLE scm (a TEXT)');
$scmDb->exec("INSERT INTO scm VALUES ('v')");
$scmSet = function ($v) use ($scmDb) {
    try { return json_encode($scmDb->setAttribute(PDO::ATTR_STATEMENT_CLASS, $v)); }
    catch (Throwable $e) { return get_class($e) . ': ' . $e->getMessage(); }
};
$scmBuild = function ($verb) use ($scmDb) {
    try {
        $s = $scmDb->$verb('SELECT * FROM scm');
        return json_encode([get_class($s), $s->z ?? null]);
    } catch (Throwable $e) { return get_class($e) . ': ' . $e->getMessage(); }
};
$scmGet = function () use ($scmDb) { return json_encode($scmDb->getAttribute(PDO::ATTR_STATEMENT_CLASS)); };

echo $scmSet(['ScmPublic']), "\n", $scmGet(), "\n";
echo $scmSet(['ScmNotAStatement']), "\n", $scmGet(), "\n";
echo $scmSet(['NoSuchScmClass']), "\n", $scmGet(), "\n";
echo $scmSet(['ScmProt']), "\n", $scmGet(), "\n", $scmBuild('query'), "\n", $scmBuild('prepare'), "\n";
echo $scmSet(['ScmProt', ['ARG']]), "\n", $scmGet(), "\n", $scmBuild('query'), "\n";
echo $scmSet(['ScmPriv', ['P']]), "\n", $scmGet(), "\n", $scmBuild('query'), "\n";
echo $scmSet(['ScmPlain', ['X']]), "\n", $scmGet(), "\n", $scmBuild('query'), "\n";
echo $scmSet(['ScmPlain']), "\n", $scmGet(), "\n", $scmBuild('query'), "\n";
/* a bad constructor_args leaves the CLASS set and the arguments dropped */
echo $scmSet(['ScmProt', ['KEPT']]), "\n", $scmGet(), "\n";
echo $scmSet(['ScmPriv', 5]), "\n", $scmGet(), "\n";
echo $scmSet([]), "\n", $scmGet(), "\n";
echo $scmSet('ScmProt'), "\n", $scmGet(), "\n";
/* back to php's own class */
echo $scmSet(['PDOStatement']), "\n", $scmGet(), "\n", $scmBuild('query'), "\n";

/* FETCH_CLASS builds through the same engine dispatch */
$scmSt = $scmDb->query('SELECT * FROM scm');
$scmSt->setFetchMode(PDO::FETCH_CLASS, 'ScmFetchPriv', ['CTOR']);
$scmRow = $scmSt->fetch();
var_dump(get_class($scmRow), $scmRow->a, $scmRow->z);
?>
--EXPECT--
TypeError: PDO::setAttribute(): Argument #2 ($value) User-supplied statement class cannot have a public constructor
["PDOStatement"]
TypeError: PDO::setAttribute(): Argument #2 ($value) PDO::ATTR_STATEMENT_CLASS class must be derived from PDOStatement
["PDOStatement"]
TypeError: PDO::setAttribute(): Argument #2 ($value) PDO::ATTR_STATEMENT_CLASS class must be a valid class
["PDOStatement"]
true
["ScmProt"]
["ScmProt",null]
["ScmProt",null]
true
["ScmProt",["ARG"]]
["ScmProt","ARG"]
true
["ScmPriv",["P"]]
["ScmPriv","P"]
true
["ScmPlain",["X"]]
Error: User-supplied statement does not accept constructor arguments
true
["ScmPlain"]
["ScmPlain",null]
true
["ScmProt",["KEPT"]]
TypeError: PDO::setAttribute(): Argument #2 ($value) PDO::ATTR_STATEMENT_CLASS constructor_args must be of type ?array, array given
["ScmPriv"]
ValueError: PDO::setAttribute(): Argument #2 ($value) PDO::ATTR_STATEMENT_CLASS value must be an array with the format array(classname, constructor_args)
["ScmPriv"]
TypeError: PDO::setAttribute(): Argument #2 ($value) PDO::ATTR_STATEMENT_CLASS value must be of type array, string given
["ScmPriv"]
true
["PDOStatement"]
["PDOStatement",null]
string(12) "ScmFetchPriv"
string(1) "v"
string(4) "CTOR"
