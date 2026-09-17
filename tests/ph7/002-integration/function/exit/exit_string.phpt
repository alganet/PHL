--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
exit with string message

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

