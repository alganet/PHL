--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Output buffers are flushed at script end (normal, exit, nested, callback)
--FILE--
<?php
ob_start();
echo "AB";
ob_start();
echo "CD";
register_shutdown_function(function(){ echo "-shutdown"; });
exit(0);
?>
--EXPECT--
ABCD-shutdown
