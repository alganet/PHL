--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_required_files answers the list get_included_files answers
--FILE--
<?php
var_dump(get_required_files() === get_included_files());
var_dump(is_array(get_required_files()));
try { get_required_files(1); } catch (Throwable $reqErr) { echo get_class($reqErr), ": ", $reqErr->getMessage(), "\n"; }
?>
--EXPECT--
bool(true)
bool(true)
ArgumentCountError: get_required_files() expects exactly 0 arguments, 1 given
--CLEAN--
<?php
unset($reqErr);
