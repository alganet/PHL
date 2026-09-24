--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: mb_strstr / mb_stristr / mb_strrchr / mb_strrichr / mb_strripos / mb_substr_count
--FILE--
<?php
// The extract pair answers the REST of the haystack from the match, or the part
// before it; not found is false, whatever $before_needle says.
echo mb_strstr("héllo wörld", "ö"), "|", mb_strstr("héllo wörld", "ö", true), "\n";
var_dump(mb_strstr("héllo", "zz"), mb_strstr("héllo", "zz", true));
// an EMPTY needle matches where the direction starts from
echo "[", mb_strstr("héllo", ""), "][", mb_strstr("héllo", "", true), "][", mb_strrchr("abc", ""), "][", mb_strrchr("abc", "", true), "]\n";
// mb_stristr folds through the same tables as mb_stripos
echo mb_stristr("HÉLLO", "é"), "|", mb_stristr("héllo", "L", true), "|", mb_strrichr("aXbxc", "X"), "\n";
// mb_strrchr takes the LAST occurrence of the WHOLE needle (not of its first
// character, which is the 8-bit strrchr's rule)
echo mb_strrchr("a/b/c/d", "/"), "|", mb_strrchr("a/b/c/d", "/", true), "|", mb_strrchr("aXbXc", "Xb"), "|", mb_strrchr("aXbXc", "bX"), "\n";
// $encoding decides what a character is here too
echo bin2hex(mb_strstr("áb", "\xa1", false, "8bit")), "|", bin2hex(mb_strstr("ab\xffcd", "c", true)), "\n";

// mb_strripos completes the position quad: last match, case-folded
var_dump(mb_strripos("áéíóúé", "é"), mb_strripos("áéíóúÉ", "é"), mb_strripos("abc", ""), mb_strripos("abc", "c", 3));
try { mb_strripos("abc", "c", 4); } catch (ValueError $mbs) { echo $mbs->getMessage(), "\n"; }

// mb_substr_count counts NON-overlapping matches
var_dump(mb_substr_count("aaa", "aa"), mb_substr_count("ábábáb", "áb"), mb_substr_count("abc", "z"), mb_substr_count("ABAB", "ab"));
var_dump(mb_substr_count("áé", "á", "8bit"), mb_substr_count("aaa", "a", "8bit"));
try { mb_substr_count("abc", ""); } catch (ValueError $mbs) { echo $mbs->getMessage(), "\n"; }
?>
--EXPECT--
örld|héllo w
bool(false)
bool(false)
[héllo][][][abc]
ÉLLO|hé|xc
/d|a/b/c|XbXc|bXc
a162|61623f
int(5)
int(5)
int(3)
bool(false)
mb_strripos(): Argument #3 ($offset) must be contained in argument #1 ($haystack)
int(1)
int(3)
int(0)
int(0)
int(1)
int(3)
mb_substr_count(): Argument #2 ($needle) must not be empty
--CLEAN--
<?php
