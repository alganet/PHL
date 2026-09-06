--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
restore_exception_handler restores previous handler
--FILE--
<?php
$result = restore_exception_handler();
if ($result === false) { echo "no_handler_false\n"; } else { echo "no_handler_true\n"; }
$old = set_exception_handler(function($e) { echo "Handler: " . $e->getMessage() . "\n"; });
if ($old === null) { echo "old_handler_null\n"; } else { echo "old_handler_not_null\n"; }
$result = restore_exception_handler();
if ($result) { echo "restore_ok\n"; } else { echo "restore_failed\n"; }
?>
--EXPECTF--
%Ano_handler_true%Aold_handler_null%Arestore_ok%A
--CLEAN--
<?php
unset($result, $old);
