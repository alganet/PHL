--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
trigger_error user error triggering
--FILE--
<?php
$result = trigger_error("Test notice", E_USER_NOTICE);
if ($result) { echo "notice_ok\n"; } else { echo "notice_failed\n"; }
$result = trigger_error("Test warning", E_USER_WARNING);
if ($result) { echo "warning_ok\n"; } else { echo "warning_failed\n"; }
?>
--EXPECTF--
%ANotice:%ATest notice%Anotice_ok%AWarning:%ATest warning%Awarning_ok%A
--CLEAN--
<?php
unset($result);
