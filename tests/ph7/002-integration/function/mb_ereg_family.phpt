--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
mbstring matches, replaces and splits with its own regular expressions
--DESCRIPTION--
mbstring carries a second regex family beside preg_*, and it answers differently
at every edge: an unmatched group is `false` and not the empty string, a group the
pattern declares but never reaches is still present, the replacement grammar knows
`\1` and not `$1` -- and leaves a backreference the pattern does not declare
standing with its backslash -- and mb_split's limit counts PIECES, with zero
meaning one. The family was absent entirely, so every name here was a call to an
undefined function.
--FILE--
<?php
function t($l,$f){ try { $r=$f(); echo "$l => ",str_replace("\n",'',var_export($r,true)),"\n"; } catch (\Throwable $e){ echo "$l => ".get_class($e).": ".$e->getMessage()."\n"; } }
t('enc',fn()=>mb_regex_encoding());
t('opt',fn()=>mb_regex_set_options());
t('encset',fn()=>mb_regex_encoding('UTF-8'));
t('ereg1',function(){ $r=mb_ereg('(a)(b)?(c)','xacy',$m); return [$r,$m]; });
t('ereg2',function(){ $r=mb_ereg('z','xacy',$m); return [$r,$m]; });
t('ereg3',function(){ $r=mb_ereg('a(b)?','a',$m); return [$r,$m]; });
t('ereg-noref',fn()=>mb_ereg('a','xacy'));
t('ereg-empty',fn()=>mb_ereg('','abc',$m));
t('match1',fn()=>mb_ereg_match('ac','xacy'));
t('match2',fn()=>mb_ereg_match('xa','xacy'));
t('match-empty',fn()=>mb_ereg_match('','abc'));
t('match-opt',fn()=>mb_ereg_match('a.c',"a\nc"));
t('split1',fn()=>mb_split(',','a,b,,c'));
t('split2',fn()=>mb_split('[,;]','a,b;c'));
t('split-lim',fn()=>mb_split(',','a,b,c,d',2));
t('split-lim0',fn()=>mb_split(',','a,b,c,d',0));
t('split-nomatch',fn()=>mb_split(',','abc'));
t('split-empty',fn()=>mb_split('','abc'));
t('split-utf8',fn()=>mb_split('é','aébéc'));
t('repl1',fn()=>mb_ereg_replace('(a)(c)','[\2\1]','xacy'));
t('repl0',fn()=>mb_ereg_replace('(a)(c)','<\0>','xacy'));
t('repl-bs',fn()=>mb_ereg_replace('a','A\\\\B','xay'));
t('repl-dollar',fn()=>mb_ereg_replace('(a)','$1','xay'));
t('repl-9',fn()=>mb_ereg_replace('(a)','[\9]','xay'));
t('repl-nonum',fn()=>mb_ereg_replace('(a)','[\z]','xay'));
t('repl-empty',fn()=>mb_ereg_replace('','-','ab'));
t('repl-opt',fn()=>mb_ereg_replace('AB','X','xaby','i'));
t('repl-opt2',fn()=>mb_ereg_replace('AB','X','xaby'));
t('opt-untouched',fn()=>mb_regex_set_options());
t('eregi',function(){ $r=mb_eregi('AC','xacy',$m); return [$r,$m]; });
t('eregi-repl',fn()=>mb_eregi_replace('AC','Q','xacy'));
t('cb',fn()=>mb_ereg_replace_callback('a(c)', fn($m)=>strtoupper($m[0]).count($m), 'xacy'));
t('cb-int',fn()=>mb_ereg_replace_callback('a', fn($m)=>123, 'xay'));
t('named',function(){ mb_ereg('(?<n>a)\k<n>','aa',$m); return $m; });
t('posix',function(){ mb_ereg('[[:alpha:]]+','a1b',$m); return $m; });
t('word',function(){ mb_ereg('(\w+)','héllo wörld',$m); return $m; });
t('badpat',fn()=>@mb_ereg('(','abc'));
t('badpat-split',fn()=>@mb_split('(','abc'));
t('nonutf8',fn()=>mb_ereg('a',"\xff\xfea"));
--EXPECT--
enc => 'UTF-8'
opt => 'pr'
encset => true
ereg1 => array (  0 => true,  1 =>   array (    0 => 'ac',    1 => 'a',    2 => false,    3 => 'c',  ),)
ereg2 => array (  0 => false,  1 =>   array (  ),)
ereg3 => array (  0 => true,  1 =>   array (    0 => 'a',    1 => false,  ),)
ereg-noref => true
ereg-empty => ValueError: mb_ereg(): Argument #1 ($pattern) must not be empty
match1 => false
match2 => true
match-empty => true
match-opt => true
split1 => array (  0 => 'a',  1 => 'b',  2 => '',  3 => 'c',)
split2 => array (  0 => 'a',  1 => 'b',  2 => 'c',)
split-lim => array (  0 => 'a',  1 => 'b,c,d',)
split-lim0 => array (  0 => 'a,b,c,d',)
split-nomatch => array (  0 => 'abc',)
split-empty => array (  0 => 'abc',)
split-utf8 => array (  0 => 'a',  1 => 'b',  2 => 'c',)
repl1 => 'x[ca]y'
repl0 => 'x<ac>y'
repl-bs => 'xA\\\\By'
repl-dollar => 'x$1y'
repl-9 => 'x[\\9]y'
repl-nonum => 'x[\\z]y'
repl-empty => '-a-b-'
repl-opt => 'xXy'
repl-opt2 => 'xaby'
opt-untouched => 'pr'
eregi => array (  0 => true,  1 =>   array (    0 => 'ac',  ),)
eregi-repl => 'xQy'
cb => 'xAC2y'
cb-int => 'x123y'
named => array (  0 => 'aa',  1 => 'a',  'n' => 'a',)
posix => array (  0 => 'a',)
word => array (  0 => 'héllo',  1 => 'héllo',)
badpat => false
badpat-split => false
nonutf8 => false
