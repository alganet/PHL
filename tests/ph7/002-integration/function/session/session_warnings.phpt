--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: session error arms — php's messages on the failure paths
--FILE--
<?php
$r1 = session_destroy();
echo "destroy:", var_export($r1, true), "\n";
$r2 = session_regenerate_id();
echo "regen:", var_export($r2, true), "\n";
$r3 = session_start();
echo "start-after-output:", var_export($r3, true), "\n";
?>
--EXPECTF--
%Asession_destroy(): Trying to destroy uninitialized session%A
destroy:false
%Asession_regenerate_id(): Session ID cannot be regenerated when there is no active session%A
regen:false
%Asession_start(): Session cannot be started after headers have already been sent%A
start-after-output:false
--CLEAN--
<?php
unset($r1, $r2, $r3);
