--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
connection_status and connection_aborted answer a live connection
--FILE--
<?php
var_dump(CONNECTION_NORMAL, CONNECTION_ABORTED, CONNECTION_TIMEOUT);
var_dump(connection_status(), connection_status() === CONNECTION_NORMAL, connection_aborted());
try { connection_status(1); } catch (Throwable $connErr) { echo get_class($connErr), ": ", $connErr->getMessage(), "\n"; }
try { connection_aborted(1); } catch (Throwable $connErr) { echo get_class($connErr), ": ", $connErr->getMessage(), "\n"; }
?>
--EXPECT--
int(0)
int(1)
int(2)
int(0)
bool(true)
int(0)
ArgumentCountError: connection_status() expects exactly 0 arguments, 1 given
ArgumentCountError: connection_aborted() expects exactly 0 arguments, 1 given
--CLEAN--
<?php
unset($connErr);
