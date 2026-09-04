--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
error_log basic functionality

--FILE--
<?php
$result = error_log("Test error message");
if ($result) { echo "error_log_ok\n"; } else { echo "error_log_failed\n"; }
$result = error_log("Test with type", 0);
if ($result) { echo "error_log_type_ok\n"; } else { echo "error_log_type_failed\n"; }
?>
--EXPECT--
error_log_ok
error_log_type_ok
--CLEAN--
<?php
unset($result);
