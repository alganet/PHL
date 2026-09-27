--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
FETCH_DEFAULT as a GIVEN mode keeps the connection's default
--DESCRIPTION--
php 8.5.11 (GH-20214) changed what `pdo_stmt_setup_fetch_mode` does with
PDO::FETCH_DEFAULT: setFetchMode(PDO::FETCH_DEFAULT) and PDO::query($sql,
PDO::FETCH_DEFAULT) leave the statement on the connection's
ATTR_DEFAULT_FETCH_MODE, where before they left it on a mode nothing could fall
back from and the next fetch() refused. The screen's reset moved with it: a
refused setFetchMode() now falls to the connection's default, not to FETCH_BOTH.

FETCH_DEFAULT still takes the mode and nothing else. Older php answers every
row of this differently, so the oracle pass skips below 8.5.11.
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '8.5.11', '<')) echo 'skip Requires PHP 8.5.11+'; ?>
--FILE--
<?php
$fdDb = new PDO('sqlite::memory:');
$fdDb->exec('CREATE TABLE fd (a TEXT, b TEXT)');
$fdDb->exec("INSERT INTO fd VALUES ('FdRow','x'),('q','y')");
$fdShow = function ($v) {
    if (is_object($v)) { return get_class($v) . ' ' . json_encode(get_object_vars($v)); }
    return json_encode($v);
};
$fdRun = function () use ($fdDb, $fdShow) {
    foreach ([[], [1], ['FdRow']] as $fdI => $fdA) {
        foreach (['set', 'query'] as $fdWhich) {
            echo str_pad("$fdWhich [$fdI]", 12), ' => ';
            try {
                if ($fdWhich === 'set') {
                    $fdSt = $fdDb->query('SELECT * FROM fd');
                    $fdSt->setFetchMode(PDO::FETCH_DEFAULT, ...$fdA);
                } else {
                    $fdSt = $fdDb->query('SELECT * FROM fd', PDO::FETCH_DEFAULT, ...$fdA);
                }
                echo $fdShow($fdSt->fetch()), "\n";
            } catch (Throwable $e) {
                echo get_class($e), ': ', $e->getMessage(), "\n";
            }
        }
    }
    /* a refused mode falls to the connection's default too */
    $fdKeep = $fdDb->query('SELECT * FROM fd', PDO::FETCH_NUM);
    try { $fdKeep->setFetchMode(PDO::FETCH_COLUMN); } catch (Throwable $e) { echo get_class($e), "\n"; }
    echo 'refused     => ', $fdShow($fdKeep->fetch()), "\n";
};

echo "-- BOTH\n";
$fdRun();
$fdDb->setAttribute(PDO::ATTR_DEFAULT_FETCH_MODE, PDO::FETCH_OBJ);
echo "-- OBJ\n";
$fdRun();
$fdDb->setAttribute(PDO::ATTR_DEFAULT_FETCH_MODE, PDO::FETCH_NUM);
echo "-- NUM\n";
$fdRun();
?>
--EXPECT--
-- BOTH
set [0]      => {"a":"FdRow","0":"FdRow","b":"x","1":"x"}
query [0]    => {"a":"FdRow","0":"FdRow","b":"x","1":"x"}
set [1]      => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 2 given
query [1]    => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 3 given
set [2]      => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 2 given
query [2]    => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 3 given
ArgumentCountError
refused     => {"a":"FdRow","0":"FdRow","b":"x","1":"x"}
-- OBJ
set [0]      => stdClass {"a":"FdRow","b":"x"}
query [0]    => stdClass {"a":"FdRow","b":"x"}
set [1]      => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 2 given
query [1]    => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 3 given
set [2]      => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 2 given
query [2]    => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 3 given
ArgumentCountError
refused     => stdClass {"a":"FdRow","b":"x"}
-- NUM
set [0]      => ["FdRow","x"]
query [0]    => ["FdRow","x"]
set [1]      => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 2 given
query [1]    => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 3 given
set [2]      => ArgumentCountError: PDOStatement::setFetchMode() expects exactly 1 arguments for the fetch mode provided, 2 given
query [2]    => ArgumentCountError: PDO::query() expects exactly 2 arguments for the fetch mode provided, 3 given
ArgumentCountError
refused     => ["FdRow","x"]
