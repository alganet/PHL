--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
user_error alias for trigger_error
--FILE--
<?php
$result = user_error("Test error", E_USER_NOTICE);
if ($result) { echo "user_error_ok\n"; } else { echo "user_error_failed\n"; }
?>
--EXPECTF--
%ANotice:%ATest error%Auser_error_ok%A
--CLEAN--
<?php
unset($result);
