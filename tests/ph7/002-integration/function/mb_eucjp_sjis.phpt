--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
EUC-JP and SJIS: the two stateless Japanese framings over JIS X 0208 -- listed, detected, converted, counted and sliced
--FILE--
<?php
// Both are names mb_list_encodings() answers, in php's own order for them.
$list = mb_list_encodings();
var_dump(in_array('EUC-JP', $list), in_array('SJIS', $list));
echo json_encode(mb_encoding_aliases('EUC-JP')), "\n";
echo json_encode(mb_encoding_aliases('SJIS')), "\n";

// Every alias is a name every encoding ARGUMENT takes.
foreach (['EUC', 'EUC_JP', 'eucJP', 'x-euc-jp', 'x-sjis', 'SHIFT-JIS'] as $a) {
    printf("%-10s %d\n", $a, mb_strlen('abc', $a));
}

// The round trip, in both directions, over the three code sets each carries:
// ASCII, the halfwidth katakana, and JIS X 0208 proper.
$words = ['日本国', 'にほんご', 'ｱｲｳ', 'a1!', '円±×'];
foreach ($words as $w) {
    foreach (['EUC-JP', 'SJIS'] as $enc) {
        $b = mb_convert_encoding($w, $enc, 'UTF-8');
        printf("%-6s %-6s %-16s len=%d back=%s\n", $enc, $w, bin2hex($b),
            mb_strlen($b, $enc), mb_convert_encoding($b, 'UTF-8', $enc));
    }
}

// A character neither carries becomes the substitute character.
mb_substitute_character(0x3F);
foreach (['EUC-JP', 'SJIS'] as $enc) {
    echo $enc, " euro: ", bin2hex(mb_convert_encoding("\u{20AC}", $enc, 'UTF-8')), "\n";
}

// Detection: a candidate is scored by how many error characters it reads.
$euc  = mb_convert_encoding('にほん', 'EUC-JP', 'UTF-8');
$sjis = mb_convert_encoding('にほん', 'SJIS', 'UTF-8');
var_dump(mb_detect_encoding($euc, ['ASCII', 'EUC-JP', 'SJIS', 'UTF-8'], true));
var_dump(mb_detect_encoding($sjis, ['ASCII', 'SJIS', 'EUC-JP', 'UTF-8'], true));
var_dump(mb_detect_encoding('日本国', ['ISO-8859-1', 'EUC-JP'], true));
// An ASCII buffer is valid under both, so the FIRST candidate wins.
var_dump(mb_detect_encoding('abc', ['EUC-JP', 'SJIS'], true));
var_dump(mb_detect_encoding('abc', ['SJIS', 'EUC-JP'], true));

// A lead byte owns the byte after it even when the pair is not a character;
// a byte that cannot lead anything is one error character on its own.
foreach ([['EUC-JP', "\xa9\xa1"], ['EUC-JP', "\xff\xff"], ['EUC-JP', "\xa1"],
          ['SJIS', "\x81\x20"], ['SJIS', "\x80\x41"], ['SJIS', "\xfd\x41"]] as [$enc, $s]) {
    printf("%-6s %-6s len=%d scrub=%s check=%d\n", $enc, bin2hex($s),
        mb_strlen($s, $enc), bin2hex(mb_scrub($s, $enc)), (int)mb_check_encoding($s, $enc));
}

// A character index is not a byte offset here, so a slice walks both ends.
$b = mb_convert_encoding('あいうえお', 'EUC-JP', 'UTF-8');
var_dump(strlen($b), mb_strlen($b, 'EUC-JP'));
echo mb_convert_encoding(mb_substr($b, 1, 3, 'EUC-JP'), 'UTF-8', 'EUC-JP'), "\n";
echo mb_convert_encoding(mb_substr($b, -2, null, 'EUC-JP'), 'UTF-8', 'EUC-JP'), "\n";
var_dump(mb_strpos($b, mb_convert_encoding('う', 'EUC-JP', 'UTF-8'), 0, 'EUC-JP'));

// mb_chr()/mb_ord() speak them too, and refuse what the encoding has no cell for.
foreach (['EUC-JP', 'SJIS'] as $enc) {
    printf("%-6s chr=%s ord=%d euro=%s\n", $enc,
        bin2hex(mb_chr(0x65E5, $enc)), mb_ord(mb_chr(0x65E5, $enc), $enc),
        var_export(mb_chr(0x20AC, $enc), true));
}

// php's encoder is many-to-one where its decoder is one-to-one: the fullwidth
// tilde and its seven neighbours are written into the cell they duplicate.
foreach ([0x203E, 0x2225, 0xFF0D, 0xFF5E, 0xFFE0, 0xFFE1, 0xFFE2] as $cp) {
    printf("U+%04X euc=%s sjis=%s\n", $cp,
        bin2hex(mb_convert_encoding(mb_chr($cp, 'UTF-8'), 'EUC-JP', 'UTF-8')),
        bin2hex(mb_convert_encoding(mb_chr($cp, 'UTF-8'), 'SJIS', 'UTF-8')));
}
// ...and two more that only SJIS falls back for.
foreach ([0x00A5, 0x00AF] as $cp) {
    printf("U+%04X sjis=%s\n", $cp,
        bin2hex(mb_convert_encoding(mb_chr($cp, 'UTF-8'), 'SJIS', 'UTF-8')));
}
?>
--EXPECT--
bool(true)
bool(true)
["EUC","EUC_JP","eucJP","x-euc-jp"]
["x-sjis","SHIFT-JIS"]
EUC        3
EUC_JP     3
eucJP      3
x-euc-jp   3
x-sjis     3
SHIFT-JIS  3
EUC-JP 日本国 c6fccbdcb9f1     len=3 back=日本国
SJIS   日本国 93fa967b8d91     len=3 back=日本国
EUC-JP にほんご a4cba4dba4f3a4b4 len=4 back=にほんご
SJIS   にほんご 82c982d982f182b2 len=4 back=にほんご
EUC-JP ｱｲｳ 8eb18eb28eb3     len=3 back=ｱｲｳ
SJIS   ｱｲｳ b1b2b3           len=3 back=ｱｲｳ
EUC-JP a1!    613121           len=3 back=a1!
SJIS   a1!    613121           len=3 back=a1!
EUC-JP 円±× b1dfa1dea1df     len=3 back=円±×
SJIS   円±× 897e817d817e     len=3 back=円±×
EUC-JP euro: 3f
SJIS euro: 3f
string(6) "EUC-JP"
string(4) "SJIS"
string(10) "ISO-8859-1"
string(6) "EUC-JP"
string(4) "SJIS"
EUC-JP a9a1   len=1 scrub=3f check=0
EUC-JP ffff   len=2 scrub=3f3f check=0
EUC-JP a1     len=1 scrub=3f check=0
SJIS   8120   len=1 scrub=3f check=0
SJIS   8041   len=2 scrub=3f41 check=0
SJIS   fd41   len=2 scrub=3f41 check=0
int(10)
int(5)
いうえ
えお
int(2)
EUC-JP chr=c6fc ord=26085 euro=false
SJIS   chr=93fa ord=26085 euro=false
U+203E euc=a1b1 sjis=8150
U+2225 euc=a1c2 sjis=8161
U+FF0D euc=a1dd sjis=817c
U+FF5E euc=a1c1 sjis=8160
U+FFE0 euc=a1f1 sjis=8191
U+FFE1 euc=a1f2 sjis=8192
U+FFE2 euc=a2cc sjis=81ca
U+00A5 sjis=818f
U+00AF sjis=8150
--CLEAN--
<?php
