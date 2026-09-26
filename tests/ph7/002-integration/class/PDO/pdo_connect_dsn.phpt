--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The sqlite DSN grammar, PDO::connect()'s driver subclass, and what a failed open throws
--DESCRIPTION--
php reads a DSN up to its first colon as the DRIVER name and hands the rest to
that driver, matching the name case-sensitively -- so `SQLITE:` is not a
spelling of `sqlite:` but a driver this build does not have. Everything after
the colon is sqlite's own path grammar: a file, the empty string (a private
temporary database), `:memory:`, and the `file:...?` URI form.

Two refusals sit before the driver: a DSN with no colon at all is refused as
the ARGUMENT, and php's `uri:` form -- whose real DSN is the first line of what
the URI names -- is refused as a URI when that cannot be read. That form is
exercised here with a bare path only: `file://` in front of a Windows absolute
path is a question about the WRAPPER's syntax, not about PDO.

A failed open is the one PDO failure the error mode never routes: the
constructor throws PDOException whatever ATTR_ERRMODE was asked for, and its
$code is the driver's int rather than the SQLSTATE string a later failure
reports.
--FILE--
<?php
$dir = __DIR__ . '/pdo_connect_dsn_scratch';
@mkdir($dir);
$file = $dir . '/d.db';
@unlink($file);

function pdo_dsn_try(string $label, callable $fn): void {
    try {
        $r = $fn();
        echo $label, ' => ', is_object($r) ? get_class($r) : var_export($r, true), "\n";
    } catch (Throwable $e) {
        echo $label, ' => ', get_class($e), ': ', $e->getMessage(),
             ' code=', var_export($e->getCode(), true),
             ' info=', json_encode($e->errorInfo ?? null), "\n";
    }
}

pdo_dsn_try('memory',      fn () => new PDO('sqlite::memory:'));
pdo_dsn_try('empty path',  fn () => new PDO('sqlite:'));
pdo_dsn_try('file',        fn () => new PDO('sqlite:' . $file));
pdo_dsn_try('file: URI',   fn () => new PDO('sqlite:file::memory:?cache=shared'));
pdo_dsn_try('upper driver',fn () => new PDO('SQLITE::memory:'));
pdo_dsn_try('leading sp',  fn () => new PDO(' sqlite::memory:'));
pdo_dsn_try('no colon',    fn () => new PDO('sqlite'));
pdo_dsn_try('empty dsn',   fn () => new PDO(''));
/* A driver NOBODY ships, on purpose. `mysql:` used to stand in for "a driver
 * that is not there" and stopped being one the day the oracle's box gained
 * pdo_mysql: php then reached the network and answered a getaddrinfo failure
 * where this asks for the missing-driver refusal. The name below cannot be
 * installed, so the row measures the refusal and not the box. */
pdo_dsn_try('other driver',fn () => new PDO('nosuchdriver:host=x'));
pdo_dsn_try('unopenable',  fn () => new PDO('sqlite:/nonexistent-dir-xyz/a.db'));
/* the credentials are accepted and ignored: sqlite has no user to be */
pdo_dsn_try('with creds',  fn () => new PDO('sqlite::memory:', 'u', 'p'));

/* connect() answers the DRIVER's class, and so does the subclass's own */
pdo_dsn_try('PDO::connect',   fn () => PDO::connect('sqlite::memory:'));
pdo_dsn_try('Sqlite::connect',fn () => Pdo\Sqlite::connect('sqlite::memory:'));
pdo_dsn_try('new Pdo\Sqlite', fn () => new Pdo\Sqlite('sqlite::memory:'));
pdo_dsn_try('connect wrong',  fn () => Pdo\Sqlite::connect('nosuchdriver:host=x'));

/* the line's TERMINATOR is part of the DSN php builds, and it reaches sqlite
   as part of the path -- so a file written with a trailing newline would open
   (and leave behind) a database literally named ":memory:\n", which Windows
   cannot even name. Every DSN written here therefore ends without one. */
file_put_contents($dir . '/dsn.txt', 'sqlite::memory:');
file_put_contents($dir . '/nested.txt', 'uri:sqlite::memory:');
file_put_contents($dir . '/spaced.txt', '  sqlite::memory:');
pdo_dsn_try('uri plain',  fn () => new PDO('uri:' . $dir . '/dsn.txt'));
pdo_dsn_try('uri nested', fn () => new PDO('uri:' . $dir . '/nested.txt'));
pdo_dsn_try('uri spaced', fn () => new PDO('uri:' . $dir . '/spaced.txt'));
pdo_dsn_try('uri missing',fn () => @new PDO('uri:' . $dir . '/nope.txt'));

/* a file DSN creates the file, and the open is always throwing */
var_dump(file_exists($file));
foreach ([PDO::ERRMODE_SILENT, PDO::ERRMODE_WARNING, PDO::ERRMODE_EXCEPTION] as $m) {
    try { new PDO('sqlite:/nonexistent-dir-xyz/a.db', null, null, [PDO::ATTR_ERRMODE => $m]); echo "mode $m: no throw\n"; }
    catch (Throwable $e) { echo "mode $m: ", get_class($e), ' ', $e->getMessage(), "\n"; }
}
/* an option array is read before the open, so it is already in force */
$d = new PDO('sqlite::memory:', null, null,
    [PDO::ATTR_ERRMODE => PDO::ERRMODE_SILENT, PDO::ATTR_PERSISTENT => true, 999999 => 'ignored']);
var_dump($d->getAttribute(PDO::ATTR_ERRMODE), $d->getAttribute(PDO::ATTR_PERSISTENT));
?>
--EXPECT--
memory => PDO
empty path => PDO
file => PDO
file: URI => PDO
upper driver => PDOException: could not find driver code=0 info=null
leading sp => PDOException: could not find driver code=0 info=null
no colon => PDOException: PDO::__construct(): Argument #1 ($dsn) must be a valid data source name code=0 info=null
empty dsn => PDOException: PDO::__construct(): Argument #1 ($dsn) must be a valid data source name code=0 info=null
other driver => PDOException: could not find driver code=0 info=null
unopenable => PDOException: SQLSTATE[HY000] [14] unable to open database file code=14 info=["HY000",14,"unable to open database file"]
with creds => PDO
PDO::connect => Pdo\Sqlite
Sqlite::connect => Pdo\Sqlite
new Pdo\Sqlite => Pdo\Sqlite
connect wrong => PDOException: could not find driver code=0 info=null
uri plain => PDO
uri nested => PDOException: could not find driver code=0 info=null
uri spaced => PDOException: could not find driver code=0 info=null
uri missing => PDOException: PDO::__construct(): Argument #1 ($dsn) must be a valid data source URI code=0 info=null
bool(true)
mode 0: PDOException SQLSTATE[HY000] [14] unable to open database file
mode 1: PDOException SQLSTATE[HY000] [14] unable to open database file
mode 2: PDOException SQLSTATE[HY000] [14] unable to open database file
int(0)
bool(true)
--CLEAN--
<?php
$dir = __DIR__ . '/pdo_connect_dsn_scratch';
foreach (['d.db', 'dsn.txt', 'nested.txt', 'spaced.txt'] as $f) { @unlink($dir . '/' . $f); }
@rmdir($dir);
?>
