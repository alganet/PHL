--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
mbstring parks a subject and walks it, and its option string round-trips
--DESCRIPTION--
mb_ereg_search_init() parks a subject and a pattern; mb_ereg_search(),
_pos() and _regs() each run one match from the cursor and leave it past what they
matched, while _getregs(), _getpos() and _setpos() only read and write. With
nothing parked the three matchers raise Error -- the pattern is checked before the
subject -- and the readers answer 0 and false instead.

The option string is the other half: it is REPLACED wholesale by each call, keeps
the syntax letter when none is named, is answered in php's own order (i, x, the
line pair, l, n, syntax) and reports the state as it stood BEFORE the call. Its
default is "pr", which is a dot that crosses newlines and anchors that do not --
the opposite of what the letters look like they say.
--FILE--
<?php
function t($l,$f){ try { $r=$f(); echo "$l => ",str_replace("\n",'',var_export($r,true)),"\n"; } catch (\Throwable $e){ echo "$l => ".get_class($e).": ".$e->getMessage()."\n"; } }

/* Nothing has been parked yet. */
t('getpos',fn()=>mb_ereg_search_getpos());
t('getregs',fn()=>mb_ereg_search_getregs());
t('setpos0',fn()=>mb_ereg_search_setpos(0));
t('setpos1',fn()=>mb_ereg_search_setpos(1));
t('search',fn()=>mb_ereg_search());
t('searchpos',fn()=>mb_ereg_search_pos());
t('searchregs',fn()=>mb_ereg_search_regs());
t('search-pat',fn()=>mb_ereg_search('a'));

/* The cursor walks the subject, and only the three matchers move it. */
t('walk',function(){ $o=[]; $o[]=mb_ereg_search_init('xacyac','a(c)'); $o[]=mb_ereg_search_getpos();
  $o[]=mb_ereg_search(); $o[]=mb_ereg_search_getpos(); $o[]=mb_ereg_search_getregs();
  $o[]=mb_ereg_search_pos(); $o[]=mb_ereg_search_regs(); $o[]=mb_ereg_search();
  $o[]=mb_ereg_search_getregs(); $o[]=mb_ereg_search_setpos(0); $o[]=mb_ereg_search_getpos();
  $o[]=mb_ereg_search_regs(); $o[]=mb_ereg_search_pos('c'); $o[]=mb_ereg_search_getpos(); return $o; });
t('walk-mb',function(){ mb_ereg_search_init("héllo héllo","llo"); return [mb_ereg_search_pos(), mb_ereg_search_getpos()]; });
t('walk-opt',function(){ mb_ereg_search_init('xABy','ab','i'); return mb_ereg_search_regs(); });
t('walk-oob',fn()=>mb_ereg_search_setpos(99));

/* The option string round-trips, and answers what it held BEFORE the call. */
t('opts-order',function(){ $o=[]; foreach(['xilnmsr','nlxi','ms','sm','ir','ri','','pi','i','u'] as $s){ mb_regex_set_options('pr'); $o[$s]=[mb_regex_set_options($s),mb_regex_set_options()]; } mb_regex_set_options('pr'); return $o; });
t('badopt',fn()=>mb_regex_set_options('Q'));
t('badenc',fn()=>mb_regex_encoding('BOGUS'));

/* The default "pr" is a dot that crosses lines and anchors that do not. */
t('sem-pr',function(){ mb_regex_set_options('pr'); return [mb_ereg('a.b',"a\nb"),mb_ereg('^b',"a\nb"),mb_ereg('a$',"a\nb"),mb_ereg('b$',"a\nb\n")]; });
t('sem-r',function(){ mb_regex_set_options('r'); $r=[mb_ereg('a.b',"a\nb"),mb_ereg('^b',"a\nb"),mb_ereg('a$',"a\nb")]; mb_regex_set_options('pr'); return $r; });

t('extfuncs',fn()=>array_values(array_filter(get_extension_funcs('mbstring'), fn($f)=>str_contains($f,'ereg')||str_contains($f,'regex')||$f==='mb_split')));
t('refl',function(){ $o=[]; foreach(['mb_ereg','mb_split','mb_ereg_search_setpos','mb_regex_set_options'] as $f){ $r=new ReflectionFunction($f); $ps=[]; foreach($r->getParameters() as $p){ $ps[]=($p->getType()?$p->getType().' ':'').($p->isPassedByReference()?'&':'').'$'.$p->getName(); } $o[$f]=implode(', ',$ps).' : '.$r->getReturnType(); } return $o; });
--EXPECT--
getpos => 0
getregs => false
setpos0 => true
setpos1 => true
search => Error: No pattern was provided
searchpos => Error: No pattern was provided
searchregs => Error: No pattern was provided
search-pat => Error: No string was provided
walk => array (  0 => true,  1 => 0,  2 => true,  3 => 3,  4 =>   array (    0 => 'ac',    1 => 'c',  ),  5 =>   array (    0 => 4,    1 => 2,  ),  6 => false,  7 => false,  8 => false,  9 => true,  10 => 0,  11 =>   array (    0 => 'ac',    1 => 'c',  ),  12 =>   array (    0 => 5,    1 => 1,  ),  13 => 6,)
walk-mb => array (  0 =>   array (    0 => 3,    1 => 3,  ),  1 => 6,)
walk-opt => array (  0 => 'AB',)
walk-oob => ValueError: mb_ereg_search_setpos(): Argument #1 ($offset) is out of range
opts-order => array (  'xilnmsr' =>   array (    0 => 'pr',    1 => 'ixplnr',  ),  'nlxi' =>   array (    0 => 'pr',    1 => 'ixlnr',  ),  'ms' =>   array (    0 => 'pr',    1 => 'pr',  ),  'sm' =>   array (    0 => 'pr',    1 => 'pr',  ),  'ir' =>   array (    0 => 'pr',    1 => 'ir',  ),  'ri' =>   array (    0 => 'pr',    1 => 'ir',  ),  '' =>   array (    0 => 'pr',    1 => 'r',  ),  'pi' =>   array (    0 => 'pr',    1 => 'ipr',  ),  'i' =>   array (    0 => 'pr',    1 => 'ir',  ),  'u' =>   array (    0 => 'pr',    1 => 'u',  ),)
badopt => ValueError: Option "Q" is not supported
badenc => ValueError: mb_regex_encoding(): Argument #1 ($encoding) must be a valid encoding, "BOGUS" given
sem-pr => array (  0 => true,  1 => false,  2 => false,  3 => true,)
sem-r => array (  0 => false,  1 => true,  2 => true,)
extfuncs => array (  0 => 'mb_regex_encoding',  1 => 'mb_ereg',  2 => 'mb_eregi',  3 => 'mb_ereg_replace',  4 => 'mb_eregi_replace',  5 => 'mb_ereg_replace_callback',  6 => 'mb_split',  7 => 'mb_ereg_match',  8 => 'mb_ereg_search',  9 => 'mb_ereg_search_pos',  10 => 'mb_ereg_search_regs',  11 => 'mb_ereg_search_init',  12 => 'mb_ereg_search_getregs',  13 => 'mb_ereg_search_getpos',  14 => 'mb_ereg_search_setpos',  15 => 'mb_regex_set_options',)
refl => array (  'mb_ereg' => 'string $pattern, string $string, &$matches : bool',  'mb_split' => 'string $pattern, string $string, int $limit : array|false',  'mb_ereg_search_setpos' => 'int $offset : bool',  'mb_regex_set_options' => '?string $options : string',)
