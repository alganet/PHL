--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_word_count formats 0/1/2 and the apostrophe/hyphen word rules
--FILE--
<?php
echo str_word_count("Hello fri3nd, you're looking good today!"), "\n";
echo json_encode(str_word_count("Hello fri3nd, you're looking good today!", 1)), "\n";
echo json_encode(str_word_count("Hello fri3nd, you're looking good today!", 2)), "\n";
// ' and - belong to words, but the string cannot start with '
// and cannot end with -
echo json_encode(str_word_count("don't -stop- me", 1)), "\n";
echo json_encode(str_word_count("'hi ho' -x ab", 1)), "\n";
echo str_word_count("me-"), "\n";
echo str_word_count("-"), "\n";
echo str_word_count("--"), "\n";
// empty input
echo str_word_count(""), "\n";
echo json_encode(str_word_count("", 2)), "\n";
?>
--EXPECT--
7
["Hello","fri","nd","you're","looking","good","today"]
{"0":"Hello","6":"fri","10":"nd","14":"you're","21":"looking","29":"good","34":"today"}
["don't","-stop-","me"]
["hi","ho'","-x","ab"]
1
0
0
0
[]
--CLEAN--
<?php
