--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_word_count charlist extends the word set, with a..z ranges
--FILE--
<?php
echo json_encode(str_word_count("Hello fri3nd!", 1, "3")), "\n";
echo json_encode(str_word_count("abc1def2ghi", 1, "0..9")), "\n";
echo json_encode(str_word_count("h_llo w~rld", 1, "_")), "\n";
// charlist can re-allow leading ' and trailing -
echo json_encode(str_word_count("'hi me-", 1, "'")), "\n";
echo json_encode(str_word_count("'hi me-", 1, "-")), "\n";
?>
--EXPECT--
["Hello","fri3nd"]
["abc1def2ghi"]
["h_llo","w","rld"]
["'hi","me"]
["hi","me-"]
--CLEAN--
<?php
