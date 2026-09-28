--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
SQLite3's four userland callbacks, and what a throw out of one does
--DESCRIPTION--
These are the verbs that run PHP from inside sqlite's own loop, and they share
one rule: a throw cannot travel back through the library's C frames. So the
status is PARKED, sqlite is told to stop, and the verb that started the step
raises it once the library has unwound -- and says nothing of its own about the
statement sqlite abandoned. What the script sees is its OWN exception out of
query().

What a callback RETURNS is mapped the way php maps it: null, int and float pass
through and everything else takes a string CAST, so a bool comes back as "1"
and an array as "Array" with php's conversion warning behind it. The string is
handed to sqlite as a C string, so it stops at the first NUL.

An aggregate's two halves both receive the running context and a row count. The
count the STEP sees is 1-based; the one the finalizer sees is always 0, which is
php's answer and not a count of anything -- and an empty group still finalizes,
with no step ever run.

A collation must answer with an INT and nothing else will do: a float, a numeric
string, a bool, null and an array are each the same complaint and a verdict of
EQUAL. The authorizer is the same rule with a different sentence, and it is
asked while sqlite COMPILES -- so a denial stops a prepare rather than a step,
and a denied SELECT fails at query() and never reaches a fetch. Both complaints
are printed under the METHOD that was running, which is the only thing that
knows it.

The callable itself is screened where php screens it -- at the argument, ahead
of every question about the connection, so an unusable one is refused even on a
closed database.
--FILE--
<?php
$sq7dv = function ($v) use (&$sq7dv) {
    if (is_array($v)) {
        $o = [];
        foreach ($v as $k => $x) { $o[] = var_export($k, true) . '=>' . $sq7dv($x); }
        return '[' . implode(', ', $o) . ']';
    }
    if (is_string($v)) { return ctype_print($v) ? '"' . $v . '"' : 'hex:' . bin2hex($v); }
    if (is_object($v)) { return 'obj:' . get_class($v); }
    return var_export($v, true);
};
$sq7show = function ($label, $fn) use ($sq7dv) {
    $notes = [];
    set_error_handler(function ($no, $str) use (&$notes) { $notes[] = "[$no] $str"; return true; });
    try { $out = $sq7dv($fn()); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    restore_error_handler();
    foreach ($notes as $n) { echo '  ', $n, "\n"; }
    echo str_pad($label, 36), ' => ', $out, "\n";
};
$sq7mk = function () {
    $d = new SQLite3(':memory:');
    $d->exec('CREATE TABLE k (a INTEGER, b TEXT)');
    $d->exec("INSERT INTO k VALUES (1,'b'),(2,'a'),(3,'c')");
    return $d;
};
$sq7db = $sq7mk();

/* a SQL function whose body is PHP */
$sq7show('createFunction', fn () => $sq7db->createFunction('sq7dbl', fn ($n) => $n * 2));
$sq7show('use it', fn () => $sq7db->query('SELECT sq7dbl(a) FROM k ORDER BY a')->fetchAll(SQLITE3_NUM));
$sq7show('any arity', function () use ($sq7db) {
    $sq7db->createFunction('sq7cnt', fn (...$a) => count($a));
    return [$sq7db->querySingle('SELECT sq7cnt()'), $sq7db->querySingle('SELECT sq7cnt(1,2,3)')]; });
$sq7show('a fixed arity', function () use ($sq7db) {
    $sq7db->createFunction('sq7two', fn ($a, $b) => "$a-$b", 2);
    return [$sq7db->querySingle('SELECT sq7two(1,2)'), $sq7db->querySingle('SELECT sq7two(1)')]; });
$sq7show('redefining replaces it', function () use ($sq7db) {
    $sq7db->createFunction('sq7dbl', fn ($n) => $n * 3);
    return $sq7db->querySingle('SELECT sq7dbl(2)'); });
$sq7show('the arity bounds', fn () => [$sq7db->createFunction('sq7b1', fn () => 1, -2),
                                       $sq7db->createFunction('sq7b2', fn () => 1, 32768),
                                       $sq7db->createFunction('sq7b3', fn () => 1, 127)]);
$sq7show('an empty name', fn () => $sq7db->createFunction('', fn () => 1));

/* what a callback RECEIVES and what it may RETURN */
$sq7show('the argument types', function () use ($sq7db) {
    $sq7db->createFunction('sq7ty', fn ($x) => gettype($x) . ':' . (is_string($x) ? bin2hex($x) : var_export($x, true)));
    $out = [];
    foreach (['1', '1.5', "'s'", 'NULL', "x'00ff'"] as $lit) { $out[] = $sq7db->querySingle("SELECT sq7ty($lit)"); }
    return $out; });
$sq7show('the return types', function () use ($sq7db) {
    $out = [];
    foreach ([['n', fn () => null], ['i', fn () => 7], ['f', fn () => 1.5], ['b', fn () => true],
              ['s', fn () => 'str'], ['z', fn () => "a\0b"], ['a', fn () => [1]]] as $p) {
        $sq7db->createFunction('sq7r' . $p[0], $p[1]);
        $out[] = [$sq7db->querySingle('SELECT typeof(sq7r' . $p[0] . '())'),
                  $sq7db->querySingle('SELECT sq7r' . $p[0] . '()')];
    }
    return $out; });

/* an aggregate: both halves get the context and a row count */
$sq7show('createAggregate', function () use ($sq7db) {
    $sq7db->createAggregate('sq7sum', function ($ctx, $rows, $v) { return ($ctx ?? 0) + $v; },
                            function ($ctx, $rows) { return "ctx=" . var_export($ctx, true) . " rows=$rows"; }, 1);
    return $sq7db->querySingle('SELECT sq7sum(a) FROM k'); });
$sq7show('the step count is 1-based', function () use ($sq7db) {
    $seen = [];
    $sq7db->createAggregate('sq7seen', function ($ctx, $rows, $v) use (&$seen) { $seen[] = $rows; return $ctx; },
                            fn ($ctx, $rows) => 'x', 1);
    $sq7db->querySingle('SELECT sq7seen(a) FROM k');
    return $seen; });
$sq7show('an empty group still finalizes', fn () => $sq7db->querySingle('SELECT sq7sum(a) FROM k WHERE a = 99'));

/* a collation: two strings in, an ORDERING out -- and only an int is one */
$sq7show('createCollation', function () use ($sq7db) {
    $sq7db->createCollation('sq7rev', fn ($x, $y) => strcmp($y, $x));
    return $sq7db->query('SELECT b FROM k ORDER BY b COLLATE sq7rev')->fetchAll(SQLITE3_NUM); });
$sq7show('a float is not an ordering', function () use ($sq7mk) {
    $d = $sq7mk();
    $d->createCollation('sq7f', fn ($x, $y) => strcmp($x, $y) + 0.0);
    return $d->query('SELECT b FROM k ORDER BY b COLLATE sq7f')->fetchAll(SQLITE3_NUM); });

/* the authorizer, asked while sqlite COMPILES */
$sq7show('what it is asked', function () use ($sq7mk) {
    $d = $sq7mk(); $seen = [];
    $d->setAuthorizer(function (...$a) use (&$seen) { $seen[] = $a; return SQLite3::OK; });
    $d->query('SELECT a FROM k');
    $d->setAuthorizer(null);
    return $seen; });
$sq7show('DENY stops the prepare', function () use ($sq7mk) {
    $d = $sq7mk();
    $d->setAuthorizer(fn ($action, ...$r) => $action === SQLite3::SELECT ? SQLite3::DENY : SQLite3::OK);
    return $d->query('SELECT a FROM k'); });
$sq7show('IGNORE blanks the column', function () use ($sq7mk) {
    $d = $sq7mk();
    $d->setAuthorizer(fn ($action, ...$r) => $action === SQLite3::READ ? SQLite3::IGNORE : SQLite3::OK);
    return $d->query('SELECT a FROM k ORDER BY a')->fetchAll(SQLITE3_NUM); });
$sq7show('anything but an int denies', function () use ($sq7mk) {
    $d = $sq7mk(); $d->setAuthorizer(fn (...$a) => '0');
    return $d->query('SELECT a FROM k'); });
$sq7show('taking it off', fn () => $sq7db->setAuthorizer(null));

/* a throw travels out of the VERB, not out of sqlite */
$sq7show('a function throws', function () use ($sq7mk) {
    $d = $sq7mk();
    $d->createFunction('sq7boom', function () { throw new RuntimeException('bang'); });
    return $d->query('SELECT sq7boom()'); });
$sq7show('a collation throws', function () use ($sq7mk) {
    $d = $sq7mk();
    $d->createCollation('sq7cx', function () { throw new LogicException('cmp'); });
    return $d->query('SELECT b FROM k ORDER BY b COLLATE sq7cx')->fetchAll(); });
$sq7show('an authorizer throws', function () use ($sq7mk) {
    $d = $sq7mk();
    $d->setAuthorizer(function () { throw new LogicException('auth'); });
    return $d->query('SELECT a FROM k'); });
$sq7show('a step throws', function () use ($sq7mk) {
    $d = $sq7mk();
    $d->createAggregate('sq7ax', function () { throw new LogicException('step'); }, fn ($c, $r) => $c, 1);
    return $d->querySingle('SELECT sq7ax(a) FROM k'); });
$sq7show('and the connection is fine', function () use ($sq7mk) {
    $d = $sq7mk();
    $d->createFunction('sq7t', function () { throw new LogicException('x'); });
    try { $d->querySingle('SELECT sq7t()'); } catch (Throwable $e) { }
    return $d->querySingle('SELECT 1'); });

/* a callable is screened before anything is asked about the connection */
$sq7show('an unusable callable', fn () => $sq7db->createFunction('sq7u', 'sq7_no_such_function'));
$sq7show('on a closed connection too', function () {
    $d = new SQLite3(':memory:'); $d->close();
    return $d->createFunction('sq7u2', 'sq7_no_such_function'); });
$sq7show('a closed connection otherwise', function () {
    $d = new SQLite3(':memory:'); $d->close();
    return $d->createFunction('sq7u3', fn () => 1); });
$sq7show('setAuthorizer names null too', fn () => $sq7db->setAuthorizer('sq7_no_such_function'));
--EXPECT--
createFunction                       => true
use it                               => [0=>[0=>2], 1=>[0=>4], 2=>[0=>6]]
any arity                            => [0=>0, 1=>3]
  [2] SQLite3::querySingle(): Unable to prepare statement: wrong number of arguments to function sq7two()
a fixed arity                        => [0=>"1-2", 1=>false]
redefining replaces it               => 6
the arity bounds                     => [0=>false, 1=>false, 2=>true]
an empty name                        => false
the argument types                   => [0=>"integer:1", 1=>"double:1.5", 2=>"string:73", 3=>"NULL:NULL", 4=>"string:00ff"]
  [2] Array to string conversion
  [2] Array to string conversion
the return types                     => [0=>[0=>"null", 1=>NULL], 1=>[0=>"integer", 1=>7], 2=>[0=>"real", 1=>1.5], 3=>[0=>"text", 1=>"1"], 4=>[0=>"text", 1=>"str"], 5=>[0=>"text", 1=>"a"], 6=>[0=>"text", 1=>"Array"]]
createAggregate                      => "ctx=6 rows=0"
the step count is 1-based            => [0=>1, 1=>2, 2=>3]
an empty group still finalizes       => "ctx=NULL rows=0"
createCollation                      => [0=>[0=>"c"], 1=>[0=>"b"], 2=>[0=>"a"]]
  [2] SQLite3::query(): An error occurred while invoking the compare callback (invalid return type).  Collation behaviour is undefined.
  [2] SQLite3::query(): An error occurred while invoking the compare callback (invalid return type).  Collation behaviour is undefined.
  [2] SQLite3Result::fetchAll(): An error occurred while invoking the compare callback (invalid return type).  Collation behaviour is undefined.
  [2] SQLite3Result::fetchAll(): An error occurred while invoking the compare callback (invalid return type).  Collation behaviour is undefined.
a float is not an ordering           => [0=>[0=>"b"], 1=>[0=>"a"], 2=>[0=>"c"]]
what it is asked                     => [0=>[0=>21, 1=>NULL, 2=>NULL, 3=>NULL, 4=>NULL], 1=>[0=>20, 1=>"k", 2=>"a", 3=>"main", 4=>NULL]]
  [2] SQLite3::query(): Unable to prepare statement: not authorized
DENY stops the prepare               => false
IGNORE blanks the column             => [0=>[0=>NULL], 1=>[0=>NULL], 2=>[0=>NULL]]
  [2] SQLite3::query(): The authorizer callback returned an invalid type: expected int
  [2] SQLite3::query(): Unable to prepare statement: not authorized
anything but an int denies           => false
taking it off                        => true
a function throws                    => RuntimeException: bang
a collation throws                   => LogicException: cmp
an authorizer throws                 => LogicException: auth
a step throws                        => LogicException: step
and the connection is fine           => 1
an unusable callable                 => TypeError: SQLite3::createFunction(): Argument #2 ($callback) must be a valid callback, function "sq7_no_such_function" not found or invalid function name
on a closed connection too           => TypeError: SQLite3::createFunction(): Argument #2 ($callback) must be a valid callback, function "sq7_no_such_function" not found or invalid function name
a closed connection otherwise        => Error: The SQLite3 object has not been correctly initialised or is already closed
setAuthorizer names null too         => TypeError: SQLite3::setAuthorizer(): Argument #1 ($callback) must be a valid callback or null, function "sq7_no_such_function" not found or invalid function name
