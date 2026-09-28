--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
SQLite3Stmt binds php-side and applies it all again at every execute
--DESCRIPTION--
The bindings are php's, not sqlite's. They are recorded on the statement and
applied at every execute -- which is what makes bindParam() read its variable
LATE, and what makes getSQL() report a binding that cannot be applied even when
it is not asked to expand anything: php binds first and looks at $expand
afterwards.

The two binders differ in more than when they read. bindValue() copies now and,
given no $type, takes the type from the VALUE it copied; bindParam() records the
caller's slot and has no value yet to take one from, so the declared default
stands and an unqualified bindParam() binds TEXT even for an integer. A NULL
value is bound as NULL whatever the type says, every other type is php's own
cast, and the cast runs on a COPY -- php never rewrites the variable bindParam()
was given.

php keys its bindings by NAME for a named bind and by POSITION for a positional
one, so `:a` and `1` naming the same parameter are two bindings that both run,
and re-binding a key MOVES it to the end -- which is the order the failures are
told in. A position past the end binds happily and fails at execute, once per
apply.

A statement carries two states a script can tell apart and php words the Error
for them with two different class NAMES: `The SQLite3 object ...` for one that
is not attached to a live connection (never prepared, or closed), and `The
SQLite3Stmt object ...` for one that has no handle -- which is what a
comment-only query prepares to, an object every accessor refuses and only
close() answers.

Two results handed out by one statement are two views of ONE cursor. close()
finalizes it under them; finalize() on a result only releases that result's own
hold, so the statement stays runnable.
--FILE--
<?php
$sq6dv = function ($v) use (&$sq6dv) {
    if (is_array($v)) {
        $o = [];
        foreach ($v as $k => $x) { $o[] = var_export($k, true) . '=>' . $sq6dv($x); }
        return '[' . implode(', ', $o) . ']';
    }
    if (is_string($v)) { return ctype_print($v) ? '"' . $v . '"' : 'hex:' . bin2hex($v); }
    if (is_object($v)) { return 'obj:' . get_class($v); }
    return var_export($v, true);
};
$sq6show = function ($label, $fn) use ($sq6dv) {
    $notes = [];
    set_error_handler(function ($no, $str) use (&$notes) { $notes[] = "[$no] $str"; return true; });
    try { $out = $sq6dv($fn()); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    restore_error_handler();
    foreach ($notes as $n) { echo '  ', $n, "\n"; }
    echo str_pad($label, 36), ' => ', $out, "\n";
};
$sq6db = new SQLite3(':memory:');
$sq6db->exec('CREATE TABLE p (a INTEGER, b TEXT)');
$sq6db->exec("INSERT INTO p VALUES (1,'x'),(2,'y'),(3,'z')");

/* what prepare answers, and the two things that are not statements */
$sq6show('prepare', fn () => $sq6db->prepare('SELECT a FROM p WHERE a = ?'));
$sq6show('prepare broken', fn () => $sq6db->prepare('NOT SQL'));
$sq6show('prepare empty', fn () => $sq6db->prepare(''));
$sq6show('prepare a comment', fn () => $sq6db->prepare('-- nothing'));

/* the statement of NOTHING: an object whose every accessor is the Error, and
   whose class NAME in that Error is not the one execute() and close() use */
$sq6show('its paramCount', fn () => $sq6db->prepare('-- nothing')->paramCount());
$sq6show('its bindValue', fn () => $sq6db->prepare('-- nothing')->bindValue(1, 1));
$sq6show('its execute', fn () => $sq6db->prepare('-- nothing')->execute());
$sq6show('its close', fn () => $sq6db->prepare('-- nothing')->close());
$sq6show('a blank statement', function () {
    return (new ReflectionClass('SQLite3Stmt'))->newInstanceWithoutConstructor()->paramCount(); });
$sq6show('new SQLite3Stmt', fn () => new SQLite3Stmt($sq6db, 'SELECT 1'));

/* the statement itself */
$sq6st = $sq6db->prepare('SELECT a, b FROM p WHERE a = :id');
$sq6show('paramCount', fn () => $sq6st->paramCount());
$sq6show('readOnly', fn () => $sq6st->readOnly());
$sq6show('readOnly of a write', fn () => $sq6db->prepare('DELETE FROM p WHERE a = 99')->readOnly());
$sq6show('busy before a walk', fn () => $sq6st->busy());
$sq6show('getSQL', fn () => $sq6st->getSQL());
$sq6show('getSQL expanded, unbound', fn () => $sq6st->getSQL(true));
$sq6show('bindValue by name', fn () => $sq6st->bindValue(':id', 2));
$sq6show('getSQL expanded, bound', fn () => $sq6st->getSQL(true));
$sq6show('execute', fn () => $sq6st->execute()->fetchArray(SQLITE3_ASSOC));
$sq6show('busy during a walk', function () use ($sq6st) {
    $r = $sq6st->execute(); $r->fetchArray(); return $sq6st->busy(); });
$sq6show('the same name without a colon', function () use ($sq6db) {
    $s = $sq6db->prepare('SELECT :id'); $s->bindValue('id', 9);
    return $s->execute()->fetchArray(SQLITE3_NUM); });
$sq6show('a name it does not carry', fn () => $sq6st->bindValue(':nope', 1));
$sq6show('the @ spelling', fn () => $sq6st->bindValue('@id', 1));
$sq6show('position zero', fn () => $sq6st->bindValue(0, 1));
$sq6show('a position past the end', fn () => $sq6st->bindValue(9, 1));
$sq6show('and it fails at execute', fn () => $sq6st->execute()->fetchArray(SQLITE3_ASSOC));
$sq6show('clear', fn () => $sq6st->clear());
$sq6show('after clear', fn () => $sq6st->execute()->fetchArray(SQLITE3_ASSOC));

/* what a bound value BECOMES */
$sq6show('inferred types', function () use ($sq6db) {
    $out = [];
    foreach ([5, 1.5, 0.0, true, false, null, 'txt', '12'] as $v) {
        $s = $sq6db->prepare('SELECT typeof(?), quote(?)');
        $s->bindValue(1, $v); $s->bindValue(2, $v);
        $out[] = $s->execute()->fetchArray(SQLITE3_NUM);
    }
    return $out; });
$sq6show('declared types', function () use ($sq6db) {
    $out = [];
    foreach ([SQLITE3_INTEGER, SQLITE3_FLOAT, SQLITE3_TEXT, SQLITE3_BLOB, SQLITE3_NULL] as $t) {
        $s = $sq6db->prepare('SELECT typeof(?), quote(?)');
        $s->bindValue(1, '17abc', $t); $s->bindValue(2, '17abc', $t);
        $out[] = $s->execute()->fetchArray(SQLITE3_NUM);
    }
    return $out; });
$sq6show('null beats the declared type', function () use ($sq6db) {
    $s = $sq6db->prepare('SELECT typeof(?)'); $s->bindValue(1, null, SQLITE3_INTEGER);
    return $s->execute()->fetchArray(SQLITE3_NUM); });
$sq6show('an array is php\'s conversion', function () use ($sq6db) {
    $s = $sq6db->prepare('SELECT quote(?)'); $s->bindValue(1, [1, 2]);
    return $s->execute()->fetchArray(SQLITE3_NUM); });
$sq6show('an object is php\'s Error', function () use ($sq6db) {
    $s = $sq6db->prepare('SELECT quote(?)'); $s->bindValue(1, new stdClass());
    return $s->execute()->fetchArray(SQLITE3_NUM); });

/* bindParam reads its variable LATE, and has no value to infer a type from */
$sq6show('bindParam is by reference', function () use ($sq6db) {
    $s = $sq6db->prepare('SELECT ?');
    $v = 1; $s->bindParam(1, $v); $v = 2;
    return $s->execute()->fetchArray(SQLITE3_NUM); });
$sq6show('bindParam defaults to TEXT', function () use ($sq6db) {
    $s = $sq6db->prepare('SELECT typeof(?)');
    $v = 5; $s->bindParam(1, $v);
    return $s->execute()->fetchArray(SQLITE3_NUM); });
$sq6show('and it does not rewrite it', function () use ($sq6db) {
    $s = $sq6db->prepare('SELECT ?');
    $v = [1]; $s->bindParam(1, $v); @$s->execute();
    return $v; });

/* rebinding a key moves it to the END of the run, which is the order the
   failures come out in */
$sq6show('the order failures are told in', function () use ($sq6db) {
    $s = $sq6db->prepare('SELECT :a');
    foreach ([99, 2, 99] as $p) { $s->bindValue($p, 1); }
    $notes = [];
    set_error_handler(function ($no, $str) use (&$notes) {
        if (preg_match('/number (\d+)/', $str, $m)) { $notes[] = $m[1]; } return true; });
    $s->execute();
    restore_error_handler();
    return $notes; });

/* two results over one statement are two views of one cursor */
$sq6show('two results, one cursor', function () use ($sq6db) {
    $s = $sq6db->prepare('SELECT a FROM p ORDER BY a');
    $r1 = $s->execute(); $a = $r1->fetchArray(SQLITE3_NUM);
    $r2 = $s->execute(); $b = $r1->fetchArray(SQLITE3_NUM); $c = $r2->fetchArray(SQLITE3_NUM);
    return [$a, $b, $c]; });
$sq6show('close cuts a result off', function () use ($sq6db) {
    $s = $sq6db->prepare('SELECT a FROM p'); $r = $s->execute(); $s->close();
    return $r->fetchArray(); });
$sq6show('and the statement is spent', function () use ($sq6db) {
    $s = $sq6db->prepare('SELECT 1'); $s->close(); return $s->execute(); });
$sq6show('finalize leaves it runnable', function () use ($sq6db) {
    $s = $sq6db->prepare('SELECT a FROM p ORDER BY a'); $r = $s->execute(); $r->finalize();
    return $s->execute()->fetchArray(SQLITE3_NUM); });
$sq6show('a statement outlives its db', function () {
    $d = new SQLite3(':memory:'); $s = $d->prepare('SELECT 42'); unset($d);
    return $s->execute()->fetchArray(SQLITE3_NUM); });
$sq6show('execute runs a write EVERY time', function () use ($sq6db) {
    $s = $sq6db->prepare("INSERT INTO p VALUES (7,'w')");
    $s->execute(); $s->execute();
    return $sq6db->querySingle('SELECT count(*) FROM p WHERE a = 7'); });

/* the query plan the statement itself can be re-aimed at */
$sq6show('explain', function () use ($sq6db) {
    $s = $sq6db->prepare('SELECT a FROM p');
    $out = [$s->explain()];
    foreach ([1, 2, 0] as $m) { $out[] = [$s->setExplain($m), $s->explain(), $s->execute()->numColumns()]; }
    return $out; });
$sq6show('setExplain out of range', fn () => $sq6db->prepare('SELECT 1')->setExplain(3));
$sq6show('clone a statement', fn () => clone $sq6db->prepare('SELECT 1'));
$sq6show('serialize a statement', fn () => serialize($sq6db->prepare('SELECT 1')));
$sq6show('a statement shows nothing', function () use ($sq6db) {
    ob_start(); var_dump($sq6db->prepare('SELECT 1'));
    return preg_replace('/#\d+/', '#N', ob_get_clean()); });
--EXPECT--
prepare                              => obj:SQLite3Stmt
  [2] SQLite3::prepare(): Unable to prepare statement: near "NOT": syntax error
prepare broken                       => false
prepare empty                        => false
prepare a comment                    => obj:SQLite3Stmt
its paramCount                       => Error: The SQLite3Stmt object has not been correctly initialised or is already closed
its bindValue                        => Error: The SQLite3Stmt object has not been correctly initialised or is already closed
  [2] SQLite3Stmt::execute(): Unable to execute statement: out of memory
its execute                          => false
its close                            => true
a blank statement                    => Error: The SQLite3 object has not been correctly initialised or is already closed
new SQLite3Stmt                      => Error: Call to private SQLite3Stmt::__construct() from global scope
paramCount                           => 1
readOnly                             => true
readOnly of a write                  => false
busy before a walk                   => false
getSQL                               => "SELECT a, b FROM p WHERE a = :id"
getSQL expanded, unbound             => "SELECT a, b FROM p WHERE a = NULL"
bindValue by name                    => true
getSQL expanded, bound               => "SELECT a, b FROM p WHERE a = 2"
execute                              => ['a'=>2, 'b'=>"y"]
busy during a walk                   => true
the same name without a colon        => [0=>9]
a name it does not carry             => false
the @ spelling                       => false
position zero                        => false
a position past the end              => true
  [2] SQLite3Stmt::execute(): Unable to bind parameter number 9
and it fails at execute              => ['a'=>2, 'b'=>"y"]
clear                                => true
after clear                          => false
inferred types                       => [0=>[0=>"integer", 1=>"5"], 1=>[0=>"real", 1=>"1.5"], 2=>[0=>"real", 1=>"0.0"], 3=>[0=>"integer", 1=>"1"], 4=>[0=>"integer", 1=>"0"], 5=>[0=>"null", 1=>"NULL"], 6=>[0=>"text", 1=>"'txt'"], 7=>[0=>"text", 1=>"'12'"]]
declared types                       => [0=>[0=>"integer", 1=>"17"], 1=>[0=>"real", 1=>"17.0"], 2=>[0=>"text", 1=>"'17abc'"], 3=>[0=>"blob", 1=>"X'3137616263'"], 4=>[0=>"null", 1=>"NULL"]]
null beats the declared type         => [0=>"null"]
  [2] Array to string conversion
an array is php's conversion         => [0=>"'Array'"]
an object is php's Error             => Error: Object of class stdClass could not be converted to string
bindParam is by reference            => [0=>"2"]
bindParam defaults to TEXT           => [0=>"text"]
  [2] Array to string conversion
and it does not rewrite it           => [0=>1]
the order failures are told in       => [0=>"2", 1=>"99"]
two results, one cursor              => [0=>[0=>1], 1=>[0=>1], 2=>[0=>2]]
close cuts a result off              => Error: The SQLite3Result object has not been correctly initialised or is already closed
and the statement is spent           => Error: The SQLite3 object has not been correctly initialised or is already closed
finalize leaves it runnable          => [0=>1]
a statement outlives its db          => [0=>42]
execute runs a write EVERY time      => 2
explain                              => [0=>0, 1=>[0=>true, 1=>1, 2=>8], 2=>[0=>true, 1=>2, 2=>4], 3=>[0=>true, 1=>0, 2=>1]]
setExplain out of range              => ValueError: SQLite3Stmt::setExplain(): Argument #1 ($mode) must be one of the SQLite3Stmt::EXPLAIN_MODE_* constants
clone a statement                    => Error: Trying to clone an uncloneable object of class SQLite3Stmt
serialize a statement                => Exception: Serialization of 'SQLite3Stmt' is not allowed
a statement shows nothing            => hex:6f626a6563742853514c6974653353746d7429234e20283029207b0a7d0a
