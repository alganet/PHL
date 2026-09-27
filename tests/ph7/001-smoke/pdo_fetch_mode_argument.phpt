--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The fetch mode a verb is GIVEN: PDO::query()'s second argument is setFetchMode()
--DESCRIPTION--
php's `PDO::query($sql, $mode, ...$args)` runs setFetchMode() on the statement it
just built — the same screen, the same per-mode arity, and diagnostics that count
arguments from the verb that took them, so what setFetchMode() calls Argument #1
and #2 PDO::query() calls #2 and #3.

The screen is php's `pdo_stmt_setup_fetch_mode`, and it is entirely per MODE:
FETCH_COLUMN wants a column NUMBER (strictly an int — a float that happens to be
integral is refused) and refuses a negative one where it is given, while one
merely past the last column answers `Invalid column index` at the fetch, and only
once there is a row to read it from. FETCH_INTO wants an OBJECT. FETCH_CLASS
wants a class NAME, resolved THERE — a class that does not exist is refused where
it was named, one that cannot be INSTANTIATED (an interface, a trait, an enum, an
abstract class) is accepted there and refused where the object would be built —
and takes optional constructor arguments behind it, unless FETCH_CLASSTYPE rides
on it, which reads the class from the first column and therefore takes nothing.
FETCH_FUNC belongs to fetchAll() alone. Every other mode takes the mode and
nothing else. A base outside php's enum is `must be a bitmask of PDO::FETCH_*
constants`. What FETCH_DEFAULT does as a GIVEN mode changed in php 8.5.11, and
is pinned apart in pdo_fetch_mode_default.phpt.

FETCH_CLASSTYPE falls back to stdClass for anything the first column cannot name.
FETCH_CLASS with no class anywhere is php's own `No fetch class specified`. And
the screen CLEARS the statement before it judges — to the connection's default
(FETCH_BOTH here), not to what the statement was carrying — so a refused
setFetchMode() moves a statement that was fetching NUM onto BOTH.
--FILE--
<?php
class FmRow { public $a; public $b; public $c; function __construct($c = null) { $this->c = $c; } }
interface FmIface {}
abstract class FmAbstract {}
trait FmTrait {}
enum FmEnum { case A; }

$fmDb = new PDO('sqlite::memory:');
$fmDb->exec('CREATE TABLE fm (a TEXT, b TEXT)');
$fmDb->exec("INSERT INTO fm VALUES ('FmRow','x'),('q','y')");
$fmModes = [
    'LAZY' => PDO::FETCH_LAZY, 'ASSOC' => PDO::FETCH_ASSOC,
    'NUM' => PDO::FETCH_NUM, 'BOTH' => PDO::FETCH_BOTH, 'OBJ' => PDO::FETCH_OBJ,
    'BOUND' => PDO::FETCH_BOUND, 'COLUMN' => PDO::FETCH_COLUMN, 'CLASS' => PDO::FETCH_CLASS,
    'INTO' => PDO::FETCH_INTO, 'FUNC' => PDO::FETCH_FUNC, 'NAMED' => PDO::FETCH_NAMED,
    'CLASSTYPE' => PDO::FETCH_CLASS | PDO::FETCH_CLASSTYPE,
    'GROUP' => PDO::FETCH_ASSOC | PDO::FETCH_GROUP, 'BOGUS' => 77,
];
$fmArgs = [[], [1], ['FmRow'], [new FmRow], ['FmRow', []], ['FmRow', 5], [1, 2]];
$fmShow = function ($v) {
    if (is_object($v)) { return get_class($v) . ' ' . json_encode(get_object_vars($v)); }
    return json_encode($v);
};
foreach ($fmModes as $fmName => $fmMode) {
    foreach ($fmArgs as $fmI => $fmA) {
        foreach (['set', 'query'] as $fmWhich) {
            echo str_pad("$fmWhich $fmName [$fmI]", 22), ' => ';
            try {
                if ($fmWhich === 'set') {
                    $fmSt = $fmDb->query('SELECT * FROM fm');
                    $fmSt->setFetchMode($fmMode, ...$fmA);
                } else {
                    $fmSt = $fmDb->query('SELECT * FROM fm', $fmMode, ...$fmA);
                }
                echo $fmShow($fmSt->fetch()), "\n";
            } catch (Throwable $e) {
                echo get_class($e), ': ', $e->getMessage(), "\n";
            }
        }
    }
}

/* a name that exists but cannot be instantiated is refused where the object
 * would be built, with the engine's own sentence */
foreach (['FmIface', 'FmAbstract', 'FmTrait', 'FmEnum', 'NoSuchFmClass'] as $fmCls) {
    echo str_pad($fmCls, 16), ' => ';
    try {
        $fmSt = $fmDb->query('SELECT * FROM fm');
        $fmSt->setFetchMode(PDO::FETCH_CLASS, $fmCls);
        echo get_class($fmSt->fetch()), "\n";
    } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}

/* CLASSTYPE takes what the first column names and falls back to stdClass */
$fmDb->exec("CREATE TABLE fmc (cls TEXT, a TEXT)");
$fmDb->exec("INSERT INTO fmc VALUES ('FmRow','1'),('nope','2'),(NULL,'3'),('7','4')");
foreach ($fmDb->query('SELECT * FROM fmc')->fetchAll(PDO::FETCH_CLASS | PDO::FETCH_CLASSTYPE) as $fmObj) {
    echo get_class($fmObj), ' ', json_encode(get_object_vars($fmObj)), "\n";
}

/* the column NUMBER: its type, its sign, and the width it is read against */
foreach ([[0], [1], [2], [5], [-1], [1.0], [1.5], ['1'], [true], [null]] as $fmCol) {
    echo 'column ', json_encode($fmCol[0]), ' => ';
    try {
        $fmSt = $fmDb->query('SELECT * FROM fm', PDO::FETCH_COLUMN, $fmCol[0]);
        echo json_encode($fmSt->fetch()), "\n";
    } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}
/* a cursor with nothing left answers false before the width is looked at */
var_dump($fmDb->query('SELECT * FROM fm WHERE 0', PDO::FETCH_COLUMN, 5)->fetch());

/* a NULL mode leaves the statement on the connection's default */
var_dump($fmDb->query('SELECT * FROM fm', null)->fetch());
/* an explicit FETCH_DEFAULT at the fetch still means the statement's own */
var_dump($fmDb->query('SELECT * FROM fm')->fetch(PDO::FETCH_DEFAULT));
/* FETCH_CLASS with no class named anywhere is the layer's own refusal */
try { $fmDb->query('SELECT * FROM fm')->fetch(PDO::FETCH_CLASS); }
catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
/* a refused mode leaves the statement carrying the one it had */
$fmKeep = $fmDb->query('SELECT * FROM fm', PDO::FETCH_NUM);
try { $fmKeep->setFetchMode(PDO::FETCH_COLUMN); } catch (Throwable $e) { echo get_class($e), "\n"; }
var_dump($fmKeep->fetch());
?>
--EXPECT--
set LAZY [0]           => PDORow []
query LAZY [0]         => PDORow []
set LAZY [1]           => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 2 given
query LAZY [1]         => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 3 given
set LAZY [2]           => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 2 given
query LAZY [2]         => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 3 given
set LAZY [3]           => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 2 given
query LAZY [3]         => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 3 given
set LAZY [4]           => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 3 given
query LAZY [4]         => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 4 given
set LAZY [5]           => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 3 given
query LAZY [5]         => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 4 given
set LAZY [6]           => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 3 given
query LAZY [6]         => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 4 given
set ASSOC [0]          => {"a":"FmRow","b":"x"}
query ASSOC [0]        => {"a":"FmRow","b":"x"}
set ASSOC [1]          => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 2 given
query ASSOC [1]        => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 3 given
set ASSOC [2]          => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 2 given
query ASSOC [2]        => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 3 given
set ASSOC [3]          => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 2 given
query ASSOC [3]        => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 3 given
set ASSOC [4]          => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 3 given
query ASSOC [4]        => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 4 given
set ASSOC [5]          => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 3 given
query ASSOC [5]        => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 4 given
set ASSOC [6]          => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 3 given
query ASSOC [6]        => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 4 given
set NUM [0]            => ["FmRow","x"]
query NUM [0]          => ["FmRow","x"]
set NUM [1]            => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 2 given
query NUM [1]          => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 3 given
set NUM [2]            => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 2 given
query NUM [2]          => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 3 given
set NUM [3]            => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 2 given
query NUM [3]          => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 3 given
set NUM [4]            => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 3 given
query NUM [4]          => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 4 given
set NUM [5]            => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 3 given
query NUM [5]          => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 4 given
set NUM [6]            => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 3 given
query NUM [6]          => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 4 given
set BOTH [0]           => {"a":"FmRow","0":"FmRow","b":"x","1":"x"}
query BOTH [0]         => {"a":"FmRow","0":"FmRow","b":"x","1":"x"}
set BOTH [1]           => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 2 given
query BOTH [1]         => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 3 given
set BOTH [2]           => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 2 given
query BOTH [2]         => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 3 given
set BOTH [3]           => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 2 given
query BOTH [3]         => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 3 given
set BOTH [4]           => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 3 given
query BOTH [4]         => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 4 given
set BOTH [5]           => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 3 given
query BOTH [5]         => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 4 given
set BOTH [6]           => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 3 given
query BOTH [6]         => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 4 given
set OBJ [0]            => stdClass {"a":"FmRow","b":"x"}
query OBJ [0]          => stdClass {"a":"FmRow","b":"x"}
set OBJ [1]            => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 2 given
query OBJ [1]          => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 3 given
set OBJ [2]            => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 2 given
query OBJ [2]          => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 3 given
set OBJ [3]            => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 2 given
query OBJ [3]          => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 3 given
set OBJ [4]            => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 3 given
query OBJ [4]          => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 4 given
set OBJ [5]            => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 3 given
query OBJ [5]          => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 4 given
set OBJ [6]            => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 3 given
query OBJ [6]          => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 4 given
set BOUND [0]          => true
query BOUND [0]        => true
set BOUND [1]          => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 2 given
query BOUND [1]        => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 3 given
set BOUND [2]          => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 2 given
query BOUND [2]        => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 3 given
set BOUND [3]          => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 2 given
query BOUND [3]        => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 3 given
set BOUND [4]          => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 3 given
query BOUND [4]        => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 4 given
set BOUND [5]          => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 3 given
query BOUND [5]        => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 4 given
set BOUND [6]          => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 3 given
query BOUND [6]        => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 4 given
set COLUMN [0]         => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 2 arguments for the fetch mode provided, 1 given
query COLUMN [0]       => ArgumentCountError: PDO::query() expects exactly 3 arguments for the fetch mode provided, 2 given
set COLUMN [1]         => "x"
query COLUMN [1]       => "x"
set COLUMN [2]         => TypeError: PDOStatement::setFetchMode(): Argument #2 must be of type int, string given
query COLUMN [2]       => TypeError: PDO::query(): Argument #3 must be of type int, string given
set COLUMN [3]         => TypeError: PDOStatement::setFetchMode(): Argument #2 must be of type int, FmRow given
query COLUMN [3]       => TypeError: PDO::query(): Argument #3 must be of type int, FmRow given
set COLUMN [4]         => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 2 arguments for the fetch mode provided, 3 given
query COLUMN [4]       => ArgumentCountError: PDO::query() expects exactly 3 arguments for the fetch mode provided, 4 given
set COLUMN [5]         => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 2 arguments for the fetch mode provided, 3 given
query COLUMN [5]       => ArgumentCountError: PDO::query() expects exactly 3 arguments for the fetch mode provided, 4 given
set COLUMN [6]         => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 2 arguments for the fetch mode provided, 3 given
query COLUMN [6]       => ArgumentCountError: PDO::query() expects exactly 3 arguments for the fetch mode provided, 4 given
set CLASS [0]          => ArgumentCountError: PDOStatement::setFetchMode() expects at least 2 arguments for the fetch mode provided, 1 given
query CLASS [0]        => ArgumentCountError: PDO::query() expects at least 3 arguments for the fetch mode provided, 2 given
set CLASS [1]          => TypeError: PDOStatement::setFetchMode(): Argument #2 must be of type string, int given
query CLASS [1]        => TypeError: PDO::query(): Argument #3 must be of type string, int given
set CLASS [2]          => FmRow {"a":"FmRow","b":"x","c":null}
query CLASS [2]        => FmRow {"a":"FmRow","b":"x","c":null}
set CLASS [3]          => TypeError: PDOStatement::setFetchMode(): Argument #2 must be of type string, FmRow given
query CLASS [3]        => TypeError: PDO::query(): Argument #3 must be of type string, FmRow given
set CLASS [4]          => FmRow {"a":"FmRow","b":"x","c":null}
query CLASS [4]        => FmRow {"a":"FmRow","b":"x","c":null}
set CLASS [5]          => TypeError: PDOStatement::setFetchMode(): Argument #3 must be of type ?array, int given
query CLASS [5]        => TypeError: PDO::query(): Argument #4 must be of type ?array, int given
set CLASS [6]          => TypeError: PDOStatement::setFetchMode(): Argument #2 must be of type string, int given
query CLASS [6]        => TypeError: PDO::query(): Argument #3 must be of type string, int given
set INTO [0]           => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 2 arguments for the fetch mode provided, 1 given
query INTO [0]         => ArgumentCountError: PDO::query() expects exactly 3 arguments for the fetch mode provided, 2 given
set INTO [1]           => TypeError: PDOStatement::setFetchMode(): Argument #2 must be of type object, int given
query INTO [1]         => TypeError: PDO::query(): Argument #3 must be of type object, int given
set INTO [2]           => TypeError: PDOStatement::setFetchMode(): Argument #2 must be of type object, string given
query INTO [2]         => TypeError: PDO::query(): Argument #3 must be of type object, string given
set INTO [3]           => FmRow {"a":"FmRow","b":"x","c":null}
query INTO [3]         => FmRow {"a":"FmRow","b":"x","c":null}
set INTO [4]           => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 2 arguments for the fetch mode provided, 3 given
query INTO [4]         => ArgumentCountError: PDO::query() expects exactly 3 arguments for the fetch mode provided, 4 given
set INTO [5]           => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 2 arguments for the fetch mode provided, 3 given
query INTO [5]         => ArgumentCountError: PDO::query() expects exactly 3 arguments for the fetch mode provided, 4 given
set INTO [6]           => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 2 arguments for the fetch mode provided, 3 given
query INTO [6]         => ArgumentCountError: PDO::query() expects exactly 3 arguments for the fetch mode provided, 4 given
set FUNC [0]           => ValueError: PDOStatement::setFetchMode(): Argument #1 ($mode) PDO::FETCH_FUNC can only be used with PDOStatement::fetchAll()
query FUNC [0]         => ValueError: PDO::query(): Argument #2 ($fetchMode) PDO::FETCH_FUNC can only be used with PDOStatement::fetchAll()
set FUNC [1]           => ValueError: PDOStatement::setFetchMode(): Argument #1 ($mode) PDO::FETCH_FUNC can only be used with PDOStatement::fetchAll()
query FUNC [1]         => ValueError: PDO::query(): Argument #2 ($fetchMode) PDO::FETCH_FUNC can only be used with PDOStatement::fetchAll()
set FUNC [2]           => ValueError: PDOStatement::setFetchMode(): Argument #1 ($mode) PDO::FETCH_FUNC can only be used with PDOStatement::fetchAll()
query FUNC [2]         => ValueError: PDO::query(): Argument #2 ($fetchMode) PDO::FETCH_FUNC can only be used with PDOStatement::fetchAll()
set FUNC [3]           => ValueError: PDOStatement::setFetchMode(): Argument #1 ($mode) PDO::FETCH_FUNC can only be used with PDOStatement::fetchAll()
query FUNC [3]         => ValueError: PDO::query(): Argument #2 ($fetchMode) PDO::FETCH_FUNC can only be used with PDOStatement::fetchAll()
set FUNC [4]           => ValueError: PDOStatement::setFetchMode(): Argument #1 ($mode) PDO::FETCH_FUNC can only be used with PDOStatement::fetchAll()
query FUNC [4]         => ValueError: PDO::query(): Argument #2 ($fetchMode) PDO::FETCH_FUNC can only be used with PDOStatement::fetchAll()
set FUNC [5]           => ValueError: PDOStatement::setFetchMode(): Argument #1 ($mode) PDO::FETCH_FUNC can only be used with PDOStatement::fetchAll()
query FUNC [5]         => ValueError: PDO::query(): Argument #2 ($fetchMode) PDO::FETCH_FUNC can only be used with PDOStatement::fetchAll()
set FUNC [6]           => ValueError: PDOStatement::setFetchMode(): Argument #1 ($mode) PDO::FETCH_FUNC can only be used with PDOStatement::fetchAll()
query FUNC [6]         => ValueError: PDO::query(): Argument #2 ($fetchMode) PDO::FETCH_FUNC can only be used with PDOStatement::fetchAll()
set NAMED [0]          => {"a":"FmRow","b":"x"}
query NAMED [0]        => {"a":"FmRow","b":"x"}
set NAMED [1]          => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 2 given
query NAMED [1]        => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 3 given
set NAMED [2]          => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 2 given
query NAMED [2]        => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 3 given
set NAMED [3]          => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 2 given
query NAMED [3]        => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 3 given
set NAMED [4]          => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 3 given
query NAMED [4]        => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 4 given
set NAMED [5]          => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 3 given
query NAMED [5]        => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 4 given
set NAMED [6]          => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 3 given
query NAMED [6]        => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 4 given
set CLASSTYPE [0]      => FmRow {"a":null,"b":"x","c":null}
query CLASSTYPE [0]    => FmRow {"a":null,"b":"x","c":null}
set CLASSTYPE [1]      => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 2 given
query CLASSTYPE [1]    => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 3 given
set CLASSTYPE [2]      => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 2 given
query CLASSTYPE [2]    => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 3 given
set CLASSTYPE [3]      => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 2 given
query CLASSTYPE [3]    => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 3 given
set CLASSTYPE [4]      => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 3 given
query CLASSTYPE [4]    => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 4 given
set CLASSTYPE [5]      => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 3 given
query CLASSTYPE [5]    => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 4 given
set CLASSTYPE [6]      => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 3 given
query CLASSTYPE [6]    => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 4 given
set GROUP [0]          => {"a":"FmRow","b":"x"}
query GROUP [0]        => {"a":"FmRow","b":"x"}
set GROUP [1]          => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 2 given
query GROUP [1]        => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 3 given
set GROUP [2]          => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 2 given
query GROUP [2]        => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 3 given
set GROUP [3]          => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 2 given
query GROUP [3]        => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 3 given
set GROUP [4]          => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 3 given
query GROUP [4]        => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 4 given
set GROUP [5]          => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 3 given
query GROUP [5]        => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 4 given
set GROUP [6]          => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 3 given
query GROUP [6]        => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 4 given
set BOGUS [0]          => ValueError: PDOStatement::setFetchMode(): Argument #1 ($mode) must be a bitmask of PDO::FETCH_* constants
query BOGUS [0]        => ValueError: PDO::query(): Argument #2 ($fetchMode) must be a bitmask of PDO::FETCH_* constants
set BOGUS [1]          => ValueError: PDOStatement::setFetchMode(): Argument #1 ($mode) must be a bitmask of PDO::FETCH_* constants
query BOGUS [1]        => ValueError: PDO::query(): Argument #2 ($fetchMode) must be a bitmask of PDO::FETCH_* constants
set BOGUS [2]          => ValueError: PDOStatement::setFetchMode(): Argument #1 ($mode) must be a bitmask of PDO::FETCH_* constants
query BOGUS [2]        => ValueError: PDO::query(): Argument #2 ($fetchMode) must be a bitmask of PDO::FETCH_* constants
set BOGUS [3]          => ValueError: PDOStatement::setFetchMode(): Argument #1 ($mode) must be a bitmask of PDO::FETCH_* constants
query BOGUS [3]        => ValueError: PDO::query(): Argument #2 ($fetchMode) must be a bitmask of PDO::FETCH_* constants
set BOGUS [4]          => ValueError: PDOStatement::setFetchMode(): Argument #1 ($mode) must be a bitmask of PDO::FETCH_* constants
query BOGUS [4]        => ValueError: PDO::query(): Argument #2 ($fetchMode) must be a bitmask of PDO::FETCH_* constants
set BOGUS [5]          => ValueError: PDOStatement::setFetchMode(): Argument #1 ($mode) must be a bitmask of PDO::FETCH_* constants
query BOGUS [5]        => ValueError: PDO::query(): Argument #2 ($fetchMode) must be a bitmask of PDO::FETCH_* constants
set BOGUS [6]          => ValueError: PDOStatement::setFetchMode(): Argument #1 ($mode) must be a bitmask of PDO::FETCH_* constants
query BOGUS [6]        => ValueError: PDO::query(): Argument #2 ($fetchMode) must be a bitmask of PDO::FETCH_* constants
FmIface          => Error: Cannot instantiate interface FmIface
FmAbstract       => Error: Cannot instantiate abstract class FmAbstract
FmTrait          => Error: Cannot instantiate trait FmTrait
FmEnum           => Error: Cannot instantiate enum FmEnum
NoSuchFmClass    => TypeError: PDOStatement::setFetchMode(): Argument #2 must be a valid class
FmRow {"a":"1","b":null,"c":null}
stdClass {"a":"2"}
stdClass {"a":"3"}
stdClass {"a":"4"}
column 0 => "FmRow"
column 1 => "x"
column 2 => ValueError: Invalid column index
column 5 => ValueError: Invalid column index
column -1 => ValueError: PDO::query(): Argument #3 must be greater than or equal to 0
column 1 => TypeError: PDO::query(): Argument #3 must be of type int, float given
column 1.5 => TypeError: PDO::query(): Argument #3 must be of type int, float given
column "1" => TypeError: PDO::query(): Argument #3 must be of type int, string given
column true => TypeError: PDO::query(): Argument #3 must be of type int, true given
column null => TypeError: PDO::query(): Argument #3 must be of type int, null given
bool(false)
array(4) {
  ["a"]=>
  string(5) "FmRow"
  [0]=>
  string(5) "FmRow"
  ["b"]=>
  string(1) "x"
  [1]=>
  string(1) "x"
}
array(4) {
  ["a"]=>
  string(5) "FmRow"
  [0]=>
  string(5) "FmRow"
  ["b"]=>
  string(1) "x"
  [1]=>
  string(1) "x"
}
PDOException: SQLSTATE[HY000]: General error: No fetch class specified
ArgumentCountError
array(4) {
  ["a"]=>
  string(5) "FmRow"
  [0]=>
  string(5) "FmRow"
  ["b"]=>
  string(1) "x"
  [1]=>
  string(1) "x"
}
