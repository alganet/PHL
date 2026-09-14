--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
LibXMLError class exists with the level/code/column/message/file/line shape
--FILE--
<?php
var_dump(class_exists('LibXMLError'));
$e = new LibXMLError;
$e->level = LIBXML_ERR_FATAL;
$e->code = 77;
$e->column = 10;
$e->message = "boom\n";
$e->file = '';
$e->line = 1;
echo get_class($e), "\n";
echo $e->level, '|', $e->code, '|', $e->column, '|', trim($e->message), '|', $e->file, '|', $e->line, "\n";
?>
--EXPECT--
bool(true)
LibXMLError
3|77|10|boom||1
--CLEAN--
<?php
unset($e);
