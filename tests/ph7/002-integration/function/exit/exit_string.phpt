--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
exit with string message
--SKIPIF--
<?php
if (!function_exists('exit')) { echo 'skip: exit not available'; }
?>
--FILE--
<?php
echo "exiting_with_message\n";
exit("done");
echo "after_exit\n";
?>
--EXPECT--
exiting_with_message
done
--CLEAN--
<?php

