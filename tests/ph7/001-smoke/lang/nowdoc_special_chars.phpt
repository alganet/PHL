--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
nowdoc with special characters
--FILE--
<?php
$nowdoc = <<< 'EOD'
This is a nowdoc string
with special chars: !@#$%^&*()
and quotes: "single' and "double"
EOD;
echo $nowdoc;
?>
--EXPECT--
This is a nowdoc string
with special chars: !@#$%^&*()
and quotes: "single' and "double"
--CLEAN--
<?php
unset($nowdoc);
