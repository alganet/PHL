--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A zero-width match steps on by one CHARACTER, which under /u is not one byte
--FILE--
<?php
/* A zero-width match advances by one CHARACTER, and under a UTF pattern that is
 * the whole multi-byte sequence: stepping one BYTE lands inside it, pcre2_match
 * refuses the offset and the scan simply stops. `preg_split('/(?<!^)(?!$)/u', …)`
 * is the unicode str_split every library writes. */
$pczSubjects = ['abc', 'éÄßご', "a\xC3\xA9b", 'aé', 'éa', '', 'é'];
$pczPatterns = [
    '/(?<!^)(?!$)/u',
    '//u',
    '/(?=.)/u',
    '/(?<=[[:lower:]])(?=[[:upper:]])/u',
    '/x*/u',
    '//',
    '/(?<!^)(?!$)/',
];
foreach ($pczPatterns as $pczP) {
    foreach ($pczSubjects as $pczS) {
        echo 'split ', $pczP, ' ', json_encode($pczS), ' => ',
            json_encode(preg_split($pczP, $pczS), JSON_UNESCAPED_UNICODE), "\n";
        echo 'repl  ', $pczP, ' ', json_encode($pczS), ' => ',
            json_encode(preg_replace($pczP, '-', $pczS), JSON_UNESCAPED_UNICODE), "\n";
        echo 'cb    ', $pczP, ' ', json_encode($pczS), ' => ',
            json_encode(preg_replace_callback($pczP, fn ($m) => '[' . $m[0] . ']', $pczS), JSON_UNESCAPED_UNICODE), "\n";
        preg_match_all($pczP, $pczS, $pczM, PREG_PATTERN_ORDER);
        echo 'all   ', $pczP, ' ', json_encode($pczS), ' => ', json_encode($pczM, JSON_UNESCAPED_UNICODE), "\n";
        preg_match_all($pczP, $pczS, $pczM2, PREG_SET_ORDER | PREG_OFFSET_CAPTURE);
        echo 'set   ', $pczP, ' ', json_encode($pczS), ' => ', json_encode($pczM2, JSON_UNESCAPED_UNICODE), "\n";
    }
}
/* The unicode str_split twig writes, and its neighbours. */
echo json_encode(preg_split('/(?<!^)(?!$)/u', 'éÄ-ßご-a'), JSON_UNESCAPED_UNICODE), "\n";
echo json_encode(preg_split('//u', 'éÄß', -1, PREG_SPLIT_NO_EMPTY), JSON_UNESCAPED_UNICODE), "\n";
echo preg_replace('/(?<=\w)(?=\p{Lu})/u', ' ', 'fooBarÉBaz'), "\n";
?>
--EXPECT--
split /(?<!^)(?!$)/u "abc" => ["a","b","c"]
repl  /(?<!^)(?!$)/u "abc" => "a-b-c"
cb    /(?<!^)(?!$)/u "abc" => "a[]b[]c"
all   /(?<!^)(?!$)/u "abc" => [["",""]]
set   /(?<!^)(?!$)/u "abc" => [[["",1]],[["",2]]]
split /(?<!^)(?!$)/u "\u00e9\u00c4\u00df\u3054" => ["é","Ä","ß","ご"]
repl  /(?<!^)(?!$)/u "\u00e9\u00c4\u00df\u3054" => "é-Ä-ß-ご"
cb    /(?<!^)(?!$)/u "\u00e9\u00c4\u00df\u3054" => "é[]Ä[]ß[]ご"
all   /(?<!^)(?!$)/u "\u00e9\u00c4\u00df\u3054" => [["","",""]]
set   /(?<!^)(?!$)/u "\u00e9\u00c4\u00df\u3054" => [[["",2]],[["",4]],[["",6]]]
split /(?<!^)(?!$)/u "a\u00e9b" => ["a","é","b"]
repl  /(?<!^)(?!$)/u "a\u00e9b" => "a-é-b"
cb    /(?<!^)(?!$)/u "a\u00e9b" => "a[]é[]b"
all   /(?<!^)(?!$)/u "a\u00e9b" => [["",""]]
set   /(?<!^)(?!$)/u "a\u00e9b" => [[["",1]],[["",3]]]
split /(?<!^)(?!$)/u "a\u00e9" => ["a","é"]
repl  /(?<!^)(?!$)/u "a\u00e9" => "a-é"
cb    /(?<!^)(?!$)/u "a\u00e9" => "a[]é"
all   /(?<!^)(?!$)/u "a\u00e9" => [[""]]
set   /(?<!^)(?!$)/u "a\u00e9" => [[["",1]]]
split /(?<!^)(?!$)/u "\u00e9a" => ["é","a"]
repl  /(?<!^)(?!$)/u "\u00e9a" => "é-a"
cb    /(?<!^)(?!$)/u "\u00e9a" => "é[]a"
all   /(?<!^)(?!$)/u "\u00e9a" => [[""]]
set   /(?<!^)(?!$)/u "\u00e9a" => [[["",2]]]
split /(?<!^)(?!$)/u "" => [""]
repl  /(?<!^)(?!$)/u "" => ""
cb    /(?<!^)(?!$)/u "" => ""
all   /(?<!^)(?!$)/u "" => [[]]
set   /(?<!^)(?!$)/u "" => []
split /(?<!^)(?!$)/u "\u00e9" => ["é"]
repl  /(?<!^)(?!$)/u "\u00e9" => "é"
cb    /(?<!^)(?!$)/u "\u00e9" => "é"
all   /(?<!^)(?!$)/u "\u00e9" => [[]]
set   /(?<!^)(?!$)/u "\u00e9" => []
split //u "abc" => ["","a","b","c",""]
repl  //u "abc" => "-a-b-c-"
cb    //u "abc" => "[]a[]b[]c[]"
all   //u "abc" => [["","","",""]]
set   //u "abc" => [[["",0]],[["",1]],[["",2]],[["",3]]]
split //u "\u00e9\u00c4\u00df\u3054" => ["","é","Ä","ß","ご",""]
repl  //u "\u00e9\u00c4\u00df\u3054" => "-é-Ä-ß-ご-"
cb    //u "\u00e9\u00c4\u00df\u3054" => "[]é[]Ä[]ß[]ご[]"
all   //u "\u00e9\u00c4\u00df\u3054" => [["","","","",""]]
set   //u "\u00e9\u00c4\u00df\u3054" => [[["",0]],[["",2]],[["",4]],[["",6]],[["",9]]]
split //u "a\u00e9b" => ["","a","é","b",""]
repl  //u "a\u00e9b" => "-a-é-b-"
cb    //u "a\u00e9b" => "[]a[]é[]b[]"
all   //u "a\u00e9b" => [["","","",""]]
set   //u "a\u00e9b" => [[["",0]],[["",1]],[["",3]],[["",4]]]
split //u "a\u00e9" => ["","a","é",""]
repl  //u "a\u00e9" => "-a-é-"
cb    //u "a\u00e9" => "[]a[]é[]"
all   //u "a\u00e9" => [["","",""]]
set   //u "a\u00e9" => [[["",0]],[["",1]],[["",3]]]
split //u "\u00e9a" => ["","é","a",""]
repl  //u "\u00e9a" => "-é-a-"
cb    //u "\u00e9a" => "[]é[]a[]"
all   //u "\u00e9a" => [["","",""]]
set   //u "\u00e9a" => [[["",0]],[["",2]],[["",3]]]
split //u "" => ["",""]
repl  //u "" => "-"
cb    //u "" => "[]"
all   //u "" => [[""]]
set   //u "" => [[["",0]]]
split //u "\u00e9" => ["","é",""]
repl  //u "\u00e9" => "-é-"
cb    //u "\u00e9" => "[]é[]"
all   //u "\u00e9" => [["",""]]
set   //u "\u00e9" => [[["",0]],[["",2]]]
split /(?=.)/u "abc" => ["","a","b","c"]
repl  /(?=.)/u "abc" => "-a-b-c"
cb    /(?=.)/u "abc" => "[]a[]b[]c"
all   /(?=.)/u "abc" => [["","",""]]
set   /(?=.)/u "abc" => [[["",0]],[["",1]],[["",2]]]
split /(?=.)/u "\u00e9\u00c4\u00df\u3054" => ["","é","Ä","ß","ご"]
repl  /(?=.)/u "\u00e9\u00c4\u00df\u3054" => "-é-Ä-ß-ご"
cb    /(?=.)/u "\u00e9\u00c4\u00df\u3054" => "[]é[]Ä[]ß[]ご"
all   /(?=.)/u "\u00e9\u00c4\u00df\u3054" => [["","","",""]]
set   /(?=.)/u "\u00e9\u00c4\u00df\u3054" => [[["",0]],[["",2]],[["",4]],[["",6]]]
split /(?=.)/u "a\u00e9b" => ["","a","é","b"]
repl  /(?=.)/u "a\u00e9b" => "-a-é-b"
cb    /(?=.)/u "a\u00e9b" => "[]a[]é[]b"
all   /(?=.)/u "a\u00e9b" => [["","",""]]
set   /(?=.)/u "a\u00e9b" => [[["",0]],[["",1]],[["",3]]]
split /(?=.)/u "a\u00e9" => ["","a","é"]
repl  /(?=.)/u "a\u00e9" => "-a-é"
cb    /(?=.)/u "a\u00e9" => "[]a[]é"
all   /(?=.)/u "a\u00e9" => [["",""]]
set   /(?=.)/u "a\u00e9" => [[["",0]],[["",1]]]
split /(?=.)/u "\u00e9a" => ["","é","a"]
repl  /(?=.)/u "\u00e9a" => "-é-a"
cb    /(?=.)/u "\u00e9a" => "[]é[]a"
all   /(?=.)/u "\u00e9a" => [["",""]]
set   /(?=.)/u "\u00e9a" => [[["",0]],[["",2]]]
split /(?=.)/u "" => [""]
repl  /(?=.)/u "" => ""
cb    /(?=.)/u "" => ""
all   /(?=.)/u "" => [[]]
set   /(?=.)/u "" => []
split /(?=.)/u "\u00e9" => ["","é"]
repl  /(?=.)/u "\u00e9" => "-é"
cb    /(?=.)/u "\u00e9" => "[]é"
all   /(?=.)/u "\u00e9" => [[""]]
set   /(?=.)/u "\u00e9" => [[["",0]]]
split /(?<=[[:lower:]])(?=[[:upper:]])/u "abc" => ["abc"]
repl  /(?<=[[:lower:]])(?=[[:upper:]])/u "abc" => "abc"
cb    /(?<=[[:lower:]])(?=[[:upper:]])/u "abc" => "abc"
all   /(?<=[[:lower:]])(?=[[:upper:]])/u "abc" => [[]]
set   /(?<=[[:lower:]])(?=[[:upper:]])/u "abc" => []
split /(?<=[[:lower:]])(?=[[:upper:]])/u "\u00e9\u00c4\u00df\u3054" => ["é","Äßご"]
repl  /(?<=[[:lower:]])(?=[[:upper:]])/u "\u00e9\u00c4\u00df\u3054" => "é-Äßご"
cb    /(?<=[[:lower:]])(?=[[:upper:]])/u "\u00e9\u00c4\u00df\u3054" => "é[]Äßご"
all   /(?<=[[:lower:]])(?=[[:upper:]])/u "\u00e9\u00c4\u00df\u3054" => [[""]]
set   /(?<=[[:lower:]])(?=[[:upper:]])/u "\u00e9\u00c4\u00df\u3054" => [[["",2]]]
split /(?<=[[:lower:]])(?=[[:upper:]])/u "a\u00e9b" => ["aéb"]
repl  /(?<=[[:lower:]])(?=[[:upper:]])/u "a\u00e9b" => "aéb"
cb    /(?<=[[:lower:]])(?=[[:upper:]])/u "a\u00e9b" => "aéb"
all   /(?<=[[:lower:]])(?=[[:upper:]])/u "a\u00e9b" => [[]]
set   /(?<=[[:lower:]])(?=[[:upper:]])/u "a\u00e9b" => []
split /(?<=[[:lower:]])(?=[[:upper:]])/u "a\u00e9" => ["aé"]
repl  /(?<=[[:lower:]])(?=[[:upper:]])/u "a\u00e9" => "aé"
cb    /(?<=[[:lower:]])(?=[[:upper:]])/u "a\u00e9" => "aé"
all   /(?<=[[:lower:]])(?=[[:upper:]])/u "a\u00e9" => [[]]
set   /(?<=[[:lower:]])(?=[[:upper:]])/u "a\u00e9" => []
split /(?<=[[:lower:]])(?=[[:upper:]])/u "\u00e9a" => ["éa"]
repl  /(?<=[[:lower:]])(?=[[:upper:]])/u "\u00e9a" => "éa"
cb    /(?<=[[:lower:]])(?=[[:upper:]])/u "\u00e9a" => "éa"
all   /(?<=[[:lower:]])(?=[[:upper:]])/u "\u00e9a" => [[]]
set   /(?<=[[:lower:]])(?=[[:upper:]])/u "\u00e9a" => []
split /(?<=[[:lower:]])(?=[[:upper:]])/u "" => [""]
repl  /(?<=[[:lower:]])(?=[[:upper:]])/u "" => ""
cb    /(?<=[[:lower:]])(?=[[:upper:]])/u "" => ""
all   /(?<=[[:lower:]])(?=[[:upper:]])/u "" => [[]]
set   /(?<=[[:lower:]])(?=[[:upper:]])/u "" => []
split /(?<=[[:lower:]])(?=[[:upper:]])/u "\u00e9" => ["é"]
repl  /(?<=[[:lower:]])(?=[[:upper:]])/u "\u00e9" => "é"
cb    /(?<=[[:lower:]])(?=[[:upper:]])/u "\u00e9" => "é"
all   /(?<=[[:lower:]])(?=[[:upper:]])/u "\u00e9" => [[]]
set   /(?<=[[:lower:]])(?=[[:upper:]])/u "\u00e9" => []
split /x*/u "abc" => ["","a","b","c",""]
repl  /x*/u "abc" => "-a-b-c-"
cb    /x*/u "abc" => "[]a[]b[]c[]"
all   /x*/u "abc" => [["","","",""]]
set   /x*/u "abc" => [[["",0]],[["",1]],[["",2]],[["",3]]]
split /x*/u "\u00e9\u00c4\u00df\u3054" => ["","é","Ä","ß","ご",""]
repl  /x*/u "\u00e9\u00c4\u00df\u3054" => "-é-Ä-ß-ご-"
cb    /x*/u "\u00e9\u00c4\u00df\u3054" => "[]é[]Ä[]ß[]ご[]"
all   /x*/u "\u00e9\u00c4\u00df\u3054" => [["","","","",""]]
set   /x*/u "\u00e9\u00c4\u00df\u3054" => [[["",0]],[["",2]],[["",4]],[["",6]],[["",9]]]
split /x*/u "a\u00e9b" => ["","a","é","b",""]
repl  /x*/u "a\u00e9b" => "-a-é-b-"
cb    /x*/u "a\u00e9b" => "[]a[]é[]b[]"
all   /x*/u "a\u00e9b" => [["","","",""]]
set   /x*/u "a\u00e9b" => [[["",0]],[["",1]],[["",3]],[["",4]]]
split /x*/u "a\u00e9" => ["","a","é",""]
repl  /x*/u "a\u00e9" => "-a-é-"
cb    /x*/u "a\u00e9" => "[]a[]é[]"
all   /x*/u "a\u00e9" => [["","",""]]
set   /x*/u "a\u00e9" => [[["",0]],[["",1]],[["",3]]]
split /x*/u "\u00e9a" => ["","é","a",""]
repl  /x*/u "\u00e9a" => "-é-a-"
cb    /x*/u "\u00e9a" => "[]é[]a[]"
all   /x*/u "\u00e9a" => [["","",""]]
set   /x*/u "\u00e9a" => [[["",0]],[["",2]],[["",3]]]
split /x*/u "" => ["",""]
repl  /x*/u "" => "-"
cb    /x*/u "" => "[]"
all   /x*/u "" => [[""]]
set   /x*/u "" => [[["",0]]]
split /x*/u "\u00e9" => ["","é",""]
repl  /x*/u "\u00e9" => "-é-"
cb    /x*/u "\u00e9" => "[]é[]"
all   /x*/u "\u00e9" => [["",""]]
set   /x*/u "\u00e9" => [[["",0]],[["",2]]]
split // "abc" => ["","a","b","c",""]
repl  // "abc" => "-a-b-c-"
cb    // "abc" => "[]a[]b[]c[]"
all   // "abc" => [["","","",""]]
set   // "abc" => [[["",0]],[["",1]],[["",2]],[["",3]]]
split // "\u00e9\u00c4\u00df\u3054" => 
repl  // "\u00e9\u00c4\u00df\u3054" => 
cb    // "\u00e9\u00c4\u00df\u3054" => 
all   // "\u00e9\u00c4\u00df\u3054" => [["","","","","","","","","",""]]
set   // "\u00e9\u00c4\u00df\u3054" => [[["",0]],[["",1]],[["",2]],[["",3]],[["",4]],[["",5]],[["",6]],[["",7]],[["",8]],[["",9]]]
split // "a\u00e9b" => 
repl  // "a\u00e9b" => 
cb    // "a\u00e9b" => 
all   // "a\u00e9b" => [["","","","",""]]
set   // "a\u00e9b" => [[["",0]],[["",1]],[["",2]],[["",3]],[["",4]]]
split // "a\u00e9" => 
repl  // "a\u00e9" => 
cb    // "a\u00e9" => 
all   // "a\u00e9" => [["","","",""]]
set   // "a\u00e9" => [[["",0]],[["",1]],[["",2]],[["",3]]]
split // "\u00e9a" => 
repl  // "\u00e9a" => 
cb    // "\u00e9a" => 
all   // "\u00e9a" => [["","","",""]]
set   // "\u00e9a" => [[["",0]],[["",1]],[["",2]],[["",3]]]
split // "" => ["",""]
repl  // "" => "-"
cb    // "" => "[]"
all   // "" => [[""]]
set   // "" => [[["",0]]]
split // "\u00e9" => 
repl  // "\u00e9" => 
cb    // "\u00e9" => 
all   // "\u00e9" => [["","",""]]
set   // "\u00e9" => [[["",0]],[["",1]],[["",2]]]
split /(?<!^)(?!$)/ "abc" => ["a","b","c"]
repl  /(?<!^)(?!$)/ "abc" => "a-b-c"
cb    /(?<!^)(?!$)/ "abc" => "a[]b[]c"
all   /(?<!^)(?!$)/ "abc" => [["",""]]
set   /(?<!^)(?!$)/ "abc" => [[["",1]],[["",2]]]
split /(?<!^)(?!$)/ "\u00e9\u00c4\u00df\u3054" => 
repl  /(?<!^)(?!$)/ "\u00e9\u00c4\u00df\u3054" => 
cb    /(?<!^)(?!$)/ "\u00e9\u00c4\u00df\u3054" => 
all   /(?<!^)(?!$)/ "\u00e9\u00c4\u00df\u3054" => [["","","","","","","",""]]
set   /(?<!^)(?!$)/ "\u00e9\u00c4\u00df\u3054" => [[["",1]],[["",2]],[["",3]],[["",4]],[["",5]],[["",6]],[["",7]],[["",8]]]
split /(?<!^)(?!$)/ "a\u00e9b" => 
repl  /(?<!^)(?!$)/ "a\u00e9b" => 
cb    /(?<!^)(?!$)/ "a\u00e9b" => 
all   /(?<!^)(?!$)/ "a\u00e9b" => [["","",""]]
set   /(?<!^)(?!$)/ "a\u00e9b" => [[["",1]],[["",2]],[["",3]]]
split /(?<!^)(?!$)/ "a\u00e9" => 
repl  /(?<!^)(?!$)/ "a\u00e9" => 
cb    /(?<!^)(?!$)/ "a\u00e9" => 
all   /(?<!^)(?!$)/ "a\u00e9" => [["",""]]
set   /(?<!^)(?!$)/ "a\u00e9" => [[["",1]],[["",2]]]
split /(?<!^)(?!$)/ "\u00e9a" => 
repl  /(?<!^)(?!$)/ "\u00e9a" => 
cb    /(?<!^)(?!$)/ "\u00e9a" => 
all   /(?<!^)(?!$)/ "\u00e9a" => [["",""]]
set   /(?<!^)(?!$)/ "\u00e9a" => [[["",1]],[["",2]]]
split /(?<!^)(?!$)/ "" => [""]
repl  /(?<!^)(?!$)/ "" => ""
cb    /(?<!^)(?!$)/ "" => ""
all   /(?<!^)(?!$)/ "" => [[]]
set   /(?<!^)(?!$)/ "" => []
split /(?<!^)(?!$)/ "\u00e9" => 
repl  /(?<!^)(?!$)/ "\u00e9" => 
cb    /(?<!^)(?!$)/ "\u00e9" => 
all   /(?<!^)(?!$)/ "\u00e9" => [[""]]
set   /(?<!^)(?!$)/ "\u00e9" => [[["",1]]]
["é","Ä","-","ß","ご","-","a"]
["é","Ä","ß"]
foo Bar É Baz
