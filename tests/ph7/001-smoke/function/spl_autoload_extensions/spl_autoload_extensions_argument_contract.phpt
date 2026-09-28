--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
spl_autoload_extensions counts its arguments and refuses an array
--FILE--
<?php
try { spl_autoload_extensions([]); } catch (Throwable $splExtErr) { echo get_class($splExtErr), ": ", $splExtErr->getMessage(), "\n"; }
try { spl_autoload_extensions('a', 'b'); } catch (Throwable $splExtErr) { echo get_class($splExtErr), ": ", $splExtErr->getMessage(), "\n"; }
var_dump(spl_autoload_extensions());
?>
--EXPECT--
TypeError: spl_autoload_extensions(): Argument #1 ($file_extensions) must be of type ?string, array given
ArgumentCountError: spl_autoload_extensions() expects at most 1 argument, 2 given
string(9) ".inc,.php"
--CLEAN--
<?php
unset($splExtErr);
