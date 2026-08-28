--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: the closing tag swallows exactly one following newline
--FILE--
<?php echo "a"; ?>
<?php echo "b"; ?>

<?php echo "c"; ?>
text
<?php echo "d\n"; ?>
--EXPECT--
ab
ctext
d
--CLEAN--
<?php
