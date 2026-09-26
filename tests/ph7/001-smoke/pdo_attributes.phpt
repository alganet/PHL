--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PDO::getAttribute()/setAttribute() answer exactly what the sqlite driver carries
--DESCRIPTION--
The attribute matrix is php's own table and it has three outcomes, not two: the
five attributes the driver carries answer and are settable, the four that
DESCRIBE the connection (both versions, the driver name, persistence) are
readable but answer false to a write without saying anything, and every generic
PDO::ATTR_* another driver would implement is the IM001 refusal
"Driver does not support this function: driver does not support that attribute".

An unknown attribute number splits across the two verbs: getAttribute() refuses
it like any unsupported one, setAttribute() just answers false.

The two version attributes are deliberately not pinned here -- they report the
LINKED libsqlite3, which differs per platform.
--FILE--
<?php
$db = new PDO('sqlite::memory:');

/* php's defaults for a fresh sqlite handle */
var_dump($db->getAttribute(PDO::ATTR_ERRMODE) === PDO::ERRMODE_EXCEPTION);
var_dump($db->getAttribute(PDO::ATTR_DEFAULT_FETCH_MODE) === PDO::FETCH_BOTH);
var_dump($db->getAttribute(PDO::ATTR_CASE) === PDO::CASE_NATURAL);
var_dump($db->getAttribute(PDO::ATTR_ORACLE_NULLS) === PDO::NULL_NATURAL);
var_dump($db->getAttribute(PDO::ATTR_STRINGIFY_FETCHES));
var_dump($db->getAttribute(PDO::ATTR_PERSISTENT));
var_dump($db->getAttribute(PDO::ATTR_DRIVER_NAME));
var_dump($db->getAttribute(PDO::ATTR_STATEMENT_CLASS));
var_dump($db->getAttribute(Pdo\Sqlite::ATTR_TRANSACTION_MODE));
/* the linked library's version, so only its shape is asserted */
var_dump(is_string($db->getAttribute(PDO::ATTR_SERVER_VERSION)),
         $db->getAttribute(PDO::ATTR_SERVER_VERSION)
         === $db->getAttribute(PDO::ATTR_CLIENT_VERSION));

/* settable: the answer is true and the value is read back */
foreach ([[PDO::ATTR_ERRMODE, PDO::ERRMODE_WARNING], [PDO::ATTR_CASE, PDO::CASE_UPPER],
          [PDO::ATTR_ORACLE_NULLS, PDO::NULL_EMPTY_STRING],
          [PDO::ATTR_STRINGIFY_FETCHES, true],
          [PDO::ATTR_DEFAULT_FETCH_MODE, PDO::FETCH_ASSOC]] as $pair) {
    $d = new PDO('sqlite::memory:');
    var_dump($d->setAttribute($pair[0], $pair[1]), $d->getAttribute($pair[0]));
}

/* false, in silence: these describe the connection */
$d = new PDO('sqlite::memory:');
var_dump($d->setAttribute(PDO::ATTR_DRIVER_NAME, 'x'),
         $d->setAttribute(PDO::ATTR_SERVER_VERSION, 'x'),
         $d->setAttribute(PDO::ATTR_CLIENT_VERSION, 'x'),
         $d->setAttribute(PDO::ATTR_PERSISTENT, true),
         $d->setAttribute(99999, 1));

/* the driver carries none of these */
foreach (['ATTR_AUTOCOMMIT', 'ATTR_PREFETCH', 'ATTR_TIMEOUT', 'ATTR_SERVER_INFO',
          'ATTR_CONNECTION_STATUS', 'ATTR_CURSOR_NAME', 'ATTR_CURSOR',
          'ATTR_FETCH_TABLE_NAMES', 'ATTR_FETCH_CATALOG_NAMES', 'ATTR_MAX_COLUMN_LEN',
          'ATTR_EMULATE_PREPARES', 'ATTR_DEFAULT_STR_PARAM'] as $name) {
    try { $d->getAttribute(constant('PDO::' . $name)); echo "$name: no refusal\n"; }
    catch (PDOException $e) { echo "$name: ", $e->getMessage(), ' code=', var_export($e->getCode(), true),
        ' info=', json_encode($e->errorInfo), "\n"; }
}
try { $d->getAttribute(99999); } catch (PDOException $e) { echo 'unknown: ', $e->getMessage(), "\n"; }

/* the two validated writes */
try { $d->setAttribute(PDO::ATTR_ERRMODE, 42); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { $d->setAttribute(PDO::ATTR_CASE, 42); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { $d->setAttribute(PDO::ATTR_STATEMENT_CLASS, ['stdClass']); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { $d->setAttribute(PDO::ATTR_STATEMENT_CLASS, []); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { $d->setAttribute(PDO::ATTR_STATEMENT_CLASS, 'nope'); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
var_dump($d->setAttribute(PDO::ATTR_STATEMENT_CLASS, ['PDOStatement', ['a']]));
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
bool(false)
string(6) "sqlite"
array(1) {
  [0]=>
  string(12) "PDOStatement"
}
int(0)
bool(true)
bool(true)
bool(true)
int(1)
bool(true)
int(1)
bool(true)
int(1)
bool(true)
bool(true)
bool(true)
int(2)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
ATTR_AUTOCOMMIT: SQLSTATE[IM001]: Driver does not support this function: driver does not support that attribute code='IM001' info=["IM001",0]
ATTR_PREFETCH: SQLSTATE[IM001]: Driver does not support this function: driver does not support that attribute code='IM001' info=["IM001",0]
ATTR_TIMEOUT: SQLSTATE[IM001]: Driver does not support this function: driver does not support that attribute code='IM001' info=["IM001",0]
ATTR_SERVER_INFO: SQLSTATE[IM001]: Driver does not support this function: driver does not support that attribute code='IM001' info=["IM001",0]
ATTR_CONNECTION_STATUS: SQLSTATE[IM001]: Driver does not support this function: driver does not support that attribute code='IM001' info=["IM001",0]
ATTR_CURSOR_NAME: SQLSTATE[IM001]: Driver does not support this function: driver does not support that attribute code='IM001' info=["IM001",0]
ATTR_CURSOR: SQLSTATE[IM001]: Driver does not support this function: driver does not support that attribute code='IM001' info=["IM001",0]
ATTR_FETCH_TABLE_NAMES: SQLSTATE[IM001]: Driver does not support this function: driver does not support that attribute code='IM001' info=["IM001",0]
ATTR_FETCH_CATALOG_NAMES: SQLSTATE[IM001]: Driver does not support this function: driver does not support that attribute code='IM001' info=["IM001",0]
ATTR_MAX_COLUMN_LEN: SQLSTATE[IM001]: Driver does not support this function: driver does not support that attribute code='IM001' info=["IM001",0]
ATTR_EMULATE_PREPARES: SQLSTATE[IM001]: Driver does not support this function: driver does not support that attribute code='IM001' info=["IM001",0]
ATTR_DEFAULT_STR_PARAM: SQLSTATE[IM001]: Driver does not support this function: driver does not support that attribute code='IM001' info=["IM001",0]
unknown: SQLSTATE[IM001]: Driver does not support this function: driver does not support that attribute
ValueError: PDO::setAttribute(): Argument #2 ($value) Error mode must be one of the PDO::ERRMODE_* constants
ValueError: PDO::setAttribute(): Argument #2 ($value) Case folding mode must be one of the PDO::CASE_* constants
TypeError: PDO::setAttribute(): Argument #2 ($value) PDO::ATTR_STATEMENT_CLASS class must be derived from PDOStatement
ValueError: PDO::setAttribute(): Argument #2 ($value) PDO::ATTR_STATEMENT_CLASS value must be an array with the format array(classname, constructor_args)
TypeError: PDO::setAttribute(): Argument #2 ($value) PDO::ATTR_STATEMENT_CLASS value must be of type array, string given
bool(true)
