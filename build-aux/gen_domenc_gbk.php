<?php
/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Cuts src/ph7/vm_dom_gbk.h -- the HTML document's GBK and gb18030 indexes --
 * from php's own HTML decoder and encoder.
 *
 *   XDEBUG_MODE=off php build-aux/gen_domenc_gbk.php > src/ph7/vm_dom_gbk.h
 *
 * The oracle is `Dom\HTMLDocument` and nothing else: mbstring and iconv carry
 * GBK tables of their own that answer differently. The two names share a
 * DECODER and do not share an ENCODER, and neither fact is assumed -- both are
 * swept per label and the generator emits one table only where the two sweeps
 * came out identical, printing which it merged.
 *
 * Nothing about the framing is taken from the standard either. Which bytes
 * open a sequence is measured with (b,0xA1): a non-ASCII trail is never
 * re-read, so a lead answers ONE character whether its cell is defined or not,
 * where a byte that stands alone answers two. Which second, third and fourth
 * bytes a FOUR-byte sequence accepts is measured a byte at a time against a
 * known-good remainder, and each window is asserted to be a single run.
 *
 * The four-byte face is swept by POINTER -- all 1587600 of them -- and
 * compressed to linear runs afterwards, rather than carrying the standard's
 * range table, because an encoding of this shape has already been caught not
 * following it. Both directions are swept from the door they serve and neither
 * is inverted out of the other.
 *
 * Every sweep is comma-separated. No byte that names a cell or a sequence is
 * 0x2C and a broken sequence re-reads only an ASCII byte, so the separator
 * survives every answer; a lost one changes the part count and aborts.
 */
const T0 = 0x40, T1 = 0xFE;             /* the pair trail window swept */
const NCOL = T1 - T0 + 1;
const LEAD0 = 0x81, LEAD1 = 0xFE;
const NROW = LEAD1 - LEAD0 + 1;
const LABELS = ['GBK','gb18030'];

/** UTF-8 -> code points, without mb_substr's quadratic walk. */
function cps(string $s): array {
	$o = []; $n = strlen($s);
	for( $i = 0 ; $i < $n ; ){
		$c = ord($s[$i]);
		if( $c < 0x80 ){ $o[] = $c; $i++; }
		elseif( $c < 0xE0 ){ $o[] = (($c & 0x1F) << 6) | (ord($s[$i+1]) & 0x3F); $i += 2; }
		elseif( $c < 0xF0 ){
			$o[] = (($c & 0x0F) << 12) | ((ord($s[$i+1]) & 0x3F) << 6) | (ord($s[$i+2]) & 0x3F);
			$i += 3;
		}else{
			$o[] = (($c & 0x07) << 18) | ((ord($s[$i+1]) & 0x3F) << 12)
			     | ((ord($s[$i+2]) & 0x3F) << 6) | (ord($s[$i+3]) & 0x3F);
			$i += 4;
		}
	}
	return $o;
}
/** A code point as UTF-8, without mb_chr. */
function u8(int $cp): string {
	if( $cp < 0x80 ) return chr($cp);
	if( $cp < 0x800 ) return chr(0xC0 | ($cp >> 6)).chr(0x80 | ($cp & 0x3F));
	if( $cp < 0x10000 ){
		return chr(0xE0 | ($cp >> 12)).chr(0x80 | (($cp >> 6) & 0x3F)).chr(0x80 | ($cp & 0x3F));
	}
	return chr(0xF0 | ($cp >> 18)).chr(0x80 | (($cp >> 12) & 0x3F))
	     .chr(0x80 | (($cp >> 6) & 0x3F)).chr(0x80 | ($cp & 0x3F));
}
function fail(string $m): never { fwrite(STDERR,"$m\n"); exit(1); }
/** Decode $src under $L and split on the commas it was built with. */
function parts(string $L,string $src,int $want): array {
	$d = Dom\HTMLDocument::createFromString("<!doctype html><p>$src</p>",0,$L);
	$p = explode(',', $d->getElementsByTagName('p')[0]->textContent);
	if( count($p) - 1 !== $want ){
		fail("$L: ".(count($p)-1)." parts for $want cells");
	}
	return $p;
}
/** One run, or abort: the bytes a stage of a sequence accepts. */
function window(string $L,array $yes,string $what): array {
	$lo = 0; $hi = 0;
	foreach( $yes as $b ){
		if( $hi && $b !== $hi + 1 ){ fail("$L: the $what window is not one run"); }
		if( !$lo ){ $lo = $b; }
		$hi = $b;
	}
	if( !$lo ){ fail("$L: no byte is accepted as $what"); }
	return [$lo,$hi];
}

$out = [];
foreach( LABELS as $L ){
	/* --- which bytes open a sequence, and what the others answer alone ---- */
	$src = ''; $bs = [];
	for( $b = 0x80 ; $b <= 0xFF ; $b++ ){ $src .= chr($b).chr(0xA1).','; $bs[] = $b; }
	$p = parts($L,$src,count($bs));
	$byte = [];                         /* 0xFFFF = a lead, 0 = U+FFFD */
	foreach( $bs as $i => $b ){
		$c = cps($p[$i]);
		if( count($c) === 1 ){
			$byte[$b] = 0xFFFF;         /* the 0xA1 was eaten with it */
		}elseif( count($c) === 2 ){
			$byte[$b] = $c[0] === 0xFFFD ? 0 : $c[0];
			if( $byte[$b] > 0xFFFF ){ fail(sprintf("%s: byte %02X is outside the BMP",$L,$b)); }
		}else{
			fail(sprintf("%s: byte %02X answered %d characters",$L,$b,count($c)));
		}
	}
	$leads = [];
	foreach( $byte as $b => $v ){ if( $v === 0xFFFF ) $leads[] = $b; }
	/* --- the four-byte framing, one stage at a time ----------------------- */
	/* Each stage is measured against a remainder already known to complete a
	 * sequence, and an accepted byte is the one that eats the whole four: a
	 * refused one leaves bytes over, which show up as extra characters. */
	$stage = function(int $pos) use ($L): array {
		$src = ''; $bs = [];
		for( $b = 0x00 ; $b <= 0xFF ; $b++ ){
			if( $b === 0x2C ) continue;                 /* the separator */
			$q = [0x81,0x30,0x81,0x31]; $q[$pos] = $b;
			$src .= chr($q[0]).chr($q[1]).chr($q[2]).chr($q[3]).','; $bs[] = $b;
		}
		$p = parts($L,$src,count($bs));
		$yes = [];
		foreach( $bs as $i => $b ){ if( count(cps($p[$i])) === 1 ) $yes[] = $b; }
		return $yes;
	};
	[$b2lo,$b2hi] = window($L,$stage(1),'second byte');
	[$b3lo,$b3hi] = window($L,$stage(2),'third byte');
	[$b4lo,$b4hi] = window($L,$stage(3),'fourth byte');
	if( $b2lo !== $b4lo || $b2hi !== $b4hi ){
		fail("$L: the second and fourth byte windows differ");
	}
	$n2 = $b2hi - $b2lo + 1; $n3 = $b3hi - $b3lo + 1;
	/* --- the pairs, over every trail the four-byte form does not claim ---- */
	$cell = array_fill(0,NROW * NCOL,0);
	foreach( $leads as $Lb ){
		$src = ''; $ts = [];
		for( $t = T0 ; $t <= T1 ; $t++ ){
			if( $t >= $b2lo && $t <= $b2hi ) continue;  /* a four-byte second byte */
			$src .= chr($Lb).chr($t).','; $ts[] = $t;
		}
		$p = parts($L,$src,count($ts));
		foreach( $ts as $i => $t ){
			$c = cps($p[$i]);
			if( count($c) === 0 || $c[0] === 0xFFFD ) continue;      /* no character here */
			if( count($c) > 1 || $c[0] > 0xFFFF ){
				fail(sprintf("%s: cell %02X%02X is not one BMP code point",$L,$Lb,$t));
			}
			$cell[($Lb - LEAD0) * NCOL + $t - T0] = $c[0];
		}
	}
	/* --- and the four-byte sequences, swept by pointer -------------------- */
	/* The pointer is the sequence's ordinal in the four nested windows; what
	 * php answers for each is read here and compressed to runs below. */
	$quad = [];                          /* pointer => code point */
	foreach( $leads as $Lb ){
		$src = ''; $ks = [];
		for( $b2 = $b2lo ; $b2 <= $b2hi ; $b2++ ){
			for( $b3 = $b3lo ; $b3 <= $b3hi ; $b3++ ){
				for( $b4 = $b4lo ; $b4 <= $b4hi ; $b4++ ){
					$src .= chr($Lb).chr($b2).chr($b3).chr($b4).',';
					$ks[] = ((($Lb - LEAD0) * $n2 + $b2 - $b2lo) * $n3
					         + $b3 - $b3lo) * ($b4hi - $b4lo + 1) + $b4 - $b4lo;
				}
			}
		}
		$p = parts($L,$src,count($ks));
		foreach( $ks as $i => $k ){
			$c = cps($p[$i]);
			if( count($c) === 0 || $c[0] === 0xFFFD ) continue;
			if( count($c) > 1 ){
				fail(sprintf("%s: pointer %d is not one code point",$L,$k));
			}
			$quad[$k] = $c[0];
		}
	}
	/* --- the encoder, over every code point php can be handed ------------- */
	$d = Dom\HTMLDocument::createEmpty($L);
	$enc = [];                           /* code point => the bytes written */
	for( $base = 0x80 ; $base <= 0x10FFFF ; $base += 0x2000 ){
		$cpl = []; $txt = '';
		for( $cp = $base ; $cp <= $base + 0x1FFF && $cp <= 0x10FFFF ; $cp++ ){
			if( $cp >= 0xD800 && $cp <= 0xDFFF ) continue;
			$cpl[] = $cp; $txt .= u8($cp).',';
		}
		if( !$cpl ) continue;
		$p = explode(',', substr($d->saveHtml($d->createComment($txt)),4,-3));
		if( count($p) - 1 !== count($cpl) ){ fail("$L: encode sweep lost cells"); }
		foreach( $cpl as $i => $cp ){
			if( $p[$i] !== '?' ) $enc[$cp] = $p[$i];
		}
	}
	$out[$L] = compact('byte','cell','quad','enc') + [
		'b2lo' => $b2lo,'b2hi' => $b2hi,'b3lo' => $b3lo,'b3hi' => $b3hi,
	];
	fwrite(STDERR,sprintf("%s: %d leads, four-byte %02X..%02X/%02X..%02X, %d pairs, %d pointers, %d encoded\n",
		$L,count($leads),$b2lo,$b2hi,$b3lo,$b3hi,
		count(array_filter($cell)),count($quad),count($enc)));
}
/* --- what the two labels share --------------------------------------------- */
$G = $out['GBK']; $B = $out['gb18030'];
foreach( ['byte','cell','quad','b2lo','b2hi','b3lo','b3hi'] as $k ){
	if( $G[$k] !== $B[$k] ){ fail("GBK and gb18030 disagree on $k: not one decoder"); }
}
fwrite(STDERR,"the two decoders are identical; one table emitted\n");
/* --- the encoders, split into their two widths ---------------------------- */
/* A one- or two-byte spelling is keyed by code point for a binary search; a
 * four-byte one is a pointer, and the pointers run linearly against the code
 * points often enough to be worth runs. */
$encSplit = function(array $enc,int $b2lo,int $b2hi,int $b3lo,int $b3hi): array {
	$n2 = $b2hi - $b2lo + 1; $n3 = $b3hi - $b3lo + 1; $n4 = $n2;
	$pair = []; $quad = [];
	foreach( $enc as $cp => $g ){
		if( strlen($g) === 1 ){ $pair[$cp] = ord($g); continue; }
		if( strlen($g) === 2 ){ $pair[$cp] = (ord($g[0]) << 8) | ord($g[1]); continue; }
		if( strlen($g) !== 4 ){ fail(sprintf("U+%04X encoded to %d bytes",$cp,strlen($g))); }
		$b = array_map('ord',str_split($g));
		if( $b[0] < LEAD0 || $b[0] > LEAD1 || $b[1] < $b2lo || $b[1] > $b2hi
		 || $b[2] < $b3lo || $b[2] > $b3hi || $b[3] < $b2lo || $b[3] > $b2hi ){
			fail(sprintf("U+%04X wrote four bytes outside the swept windows",$cp));
		}
		$quad[$cp] = ((($b[0] - LEAD0) * $n2 + $b[1] - $b2lo) * $n3
		              + $b[2] - $b3lo) * $n4 + $b[3] - $b2lo;
	}
	return [$pair,$quad];
};
[$gPair,$gQuad] = $encSplit($G['enc'],$G['b2lo'],$G['b2hi'],$G['b3lo'],$G['b3hi']);
[$bPair,$bQuad] = $encSplit($B['enc'],$B['b2lo'],$B['b2hi'],$B['b3lo'],$B['b3hi']);
if( $gQuad ){ fail('GBK wrote a four-byte sequence'); }
/** A map keyed by an ascending integer, compressed to runs where both sides
 *  advance by one. */
function runs(array $m): array {
	ksort($m);
	$k0 = []; $v0 = []; $len = [];
	$pk = -2; $pv = -2;
	foreach( $m as $k => $v ){
		if( $k === $pk + 1 && $v === $pv + 1 ){
			$len[count($len) - 1]++;
		}else{
			$k0[] = $k; $v0[] = $v; $len[] = 1;
		}
		$pk = $k; $pv = $v;
	}
	return [$k0,$v0,$len];
}
[$dK,$dV,$dL] = runs($G['quad']);                    /* pointer -> code point */
[$eK,$eV,$eL] = runs($bQuad);                        /* code point -> pointer */
fwrite(STDERR,sprintf("four-byte: %d pointers in %d decode runs, %d code points in %d encode runs\n",
	count($G['quad']),count($dK),count($bQuad),count($eK)));
/* --- emit ----------------------------------------------------------------- */
$w = function(array $a,string $fmt = '0x%04X',int $per = 12){
	$o = ''; $a = array_values($a); $n = count($a);
	for( $i = 0 ; $i < $n ; $i++ ){
		$o .= ($i % $per === 0 ? "\t" : '').sprintf($fmt,$a[$i]).($i + 1 === $n ? '' : ',');
		if( $i % $per === $per - 1 || $i + 1 === $n ) $o .= "\n";
	}
	return $o;
};
$kv = function(array $m,callable $w,string $name,string $nDef){
	ksort($m);
	$bWide = max(array_keys($m)) > 0xFFFF;
	printf("static const %s %sCp[%s] = {\n%s};\n",$bWide ? 'sxu32' : 'sxu16',$name,$nDef,
		$w(array_keys($m),$bWide ? '0x%05X' : '0x%04X'));
	printf("static const sxu16 %sB[%s] = {\n%s};\n",$name,$nDef,$w(array_values($m)));
};
echo <<<'TXT'
/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The HTML document's GBK and gb18030 indexes. GENERATED -- do not hand-edit;
 * re-cut with `XDEBUG_MODE=off php build-aux/gen_domenc_gbk.php`.
 *
 * This is NOT mbstring's or iconv's GBK. Every array here is swept from
 * `Dom\HTMLDocument`, one face and one label at a time. The two labels came
 * out with the SAME decoder -- byte table, pair table and four-byte pointers
 * all identical, which the generator asserts rather than assumes -- and with
 * two DIFFERENT encoders, so the decode tables are shared and the encode ones
 * are not.
 *
 *   aDomGbkByte     what a byte 0x80..0xFF answers ALONE -- a code point, 0
 *                   where the decoder answers U+FFFD, and 0xFFFF where the
 *                   byte opens a longer sequence instead.
 *   aDomGbkCell     the PAIR table, row (lead - 0x81), column (trail - 0x40).
 *                   The columns the four-byte form claims are empty here.
 *   aDomGbkDec*     the FOUR-byte decoder: the pointer of a sequence is its
 *                   ordinal in the four byte windows, and the pointers that
 *                   name a character run linearly against their code points,
 *                   so they are stored as runs -- first pointer, first code
 *                   point, length. A pointer inside no run is U+FFFD.
 *   aDomGbkEnc*     GBK's encoder, by code point: the byte (below 0x100) or
 *                   the lead<<8|trail it writes. GBK writes no four-byte
 *                   sequence at all -- a character only they spell is `?`.
 *   aDomGb18030Enc* gb18030's, the same shape and NOT the same table: the euro
 *                   is one byte 0x80 in GBK and the pair 0xA2E3 here.
 *   aDomGb18030E4*  and its four-byte encoder, runs of code point to pointer.
 *                   NEITHER face is the other's subset. Eighteen pointers
 *                   decode to a character this face writes as a PAIR instead,
 *                   and two code points write a pointer the decoder answers
 *                   U+FFFD for -- which is why the two are swept separately
 *                   and why a round trip through them is not the identity.
 *
 * Both faces are indexed by the BYTES rather than by the standard's pointer
 * arithmetic because php's framing is not always the standard's.
 */

TXT;
printf("#define PH7_DOM_GBK_B2LO 0x%02X\n",$G['b2lo']);
printf("#define PH7_DOM_GBK_B2HI 0x%02X\n",$G['b2hi']);
printf("#define PH7_DOM_GBK_B3LO 0x%02X\n",$G['b3lo']);
printf("#define PH7_DOM_GBK_B3HI 0x%02X\n",$G['b3hi']);
printf("#define PH7_DOM_GBK_DECRUN %d\n",count($dK));
printf("#define PH7_DOM_GBK_ENC %d\n",count($gPair));
printf("#define PH7_DOM_GB18030_ENC %d\n",count($bPair));
printf("#define PH7_DOM_GB18030_E4RUN %d\n",count($eK));
printf("static const sxu16 aDomGbkByte[128] = {\n%s};\n",$w($G['byte']));
printf("static const sxu16 aDomGbkCell[%d * %d] = {\n%s};\n",NROW,NCOL,$w($G['cell']));
printf("static const sxu32 aDomGbkDecPtr[PH7_DOM_GBK_DECRUN] = {\n%s};\n",$w($dK,'%7d',10));
printf("static const sxu32 aDomGbkDecCp[PH7_DOM_GBK_DECRUN] = {\n%s};\n",$w($dV,'0x%06X',10));
printf("static const sxu32 aDomGbkDecLen[PH7_DOM_GBK_DECRUN] = {\n%s};\n",$w($dL,'%7d',10));
$kv($gPair,$w,'aDomGbkEnc','PH7_DOM_GBK_ENC');
$kv($bPair,$w,'aDomGb18030Enc','PH7_DOM_GB18030_ENC');
printf("static const sxu32 aDomGb18030E4Cp[PH7_DOM_GB18030_E4RUN] = {\n%s};\n",$w($eK,'0x%06X',10));
printf("static const sxu32 aDomGb18030E4Ptr[PH7_DOM_GB18030_E4RUN] = {\n%s};\n",$w($eV,'%7d',10));
printf("static const sxu32 aDomGb18030E4Len[PH7_DOM_GB18030_E4RUN] = {\n%s};\n",$w($eL,'%7d',10));
