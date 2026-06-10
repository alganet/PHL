--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Plain # and // comments still consume to end of line
--FILE--
<?php
# comment with [ bracket and ] more
echo "hash ok\n";
#plain comment
echo "plain ok\n";
// comment with [ bracket
echo "slash ok\n";
$x = 1; # [ trailing comment with bracket
echo $x, "\n";
?>
--EXPECT--
hash ok
plain ok
slash ok
1
--CLEAN--
<?php
?>
