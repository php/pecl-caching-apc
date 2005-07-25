<?php
/*
  +----------------------------------------------------------------------+
  | APC                                                                  |
  +----------------------------------------------------------------------+
  | Copyright (c) 2005 The PHP Group                                     |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.0 of the PHP license,       |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_0.txt.                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Ralf Becker <beckerr@php.net>                               |
  |          Rasmus Lerdorf <rasmus@php.net>                             |
  +----------------------------------------------------------------------+

   All other licensing and usage conditions are those of the PHP Group.

 */

$VERSION='$Id$';



////////// BEGIN OF CONFIG AREA ///////////////////////////////////////////////////////////

define(ADMIN_PASSWORD,'password');  	// Change this to enable the Clear Cache Command

//define(DATE_FORMAT, "d.m.Y H:i:s");	// German
define(DATE_FORMAT, 'Y/m/d H:i:s'); 	// US


////////// END OF CONFIG AREA /////////////////////////////////////////////////////////////






// rewrite $PHP_SELF to block XSS attacks
//
$PHP_SELF= isset($_SERVER['PHP_SELF']) ? htmlentities(strip_tags($_SERVER['PHP_SELF'],'')) : '';
$time = time();

// operation constants
define('OB_HOST_STATS',1);
define('OB_SYS_CACHE',2);
define('OB_USER_CACHE',3);
define('OB_VERSION_CHECK',9);

// check validity of input variables
$vardom=array(
	'CC'	=> '/^[01]$/',
	'COUNT'	=> '/^\d+$/',
	'IMG'	=> '/^[12]$/',
	'OB'	=> '/^\d+$/',
	'SCOPE'	=> '/^[AD]$/',
	'SH'	=> '/^[a-z0-9]+$/',
	'SORT1'	=> '/^[HSMCDT]$/',
	'SORT2'	=> '/^[DA]$/',
);

// default cache mode
$cache_mode='opcode';

// cache scope
$scope_list=array(
	'A' => 'cache_list',
	'D' => 'deleted_list'
);


// handle POST and GET requests
if (empty($_REQUEST)) {
	if (!empty($_GET) && !empty($_POST)) {
		$_REQUEST = array_merge($_GET, $_POST);
	} else if (!empty($_GET)) {
		$_REQUEST = $_GET;
	} else if (!empty($_POST)) {
		$_REQUEST = $_POST;
	} else {
		$_REQUEST = array();
	}
}

// check parameter syntax
foreach($vardom as $var => $dom) {
	if (!isset($_REQUEST[$var])) {
		$MYREQUEST[$var]=NULL;
		continue;
	}
	if (!is_array($_REQUEST[$var]) && preg_match($dom,$_REQUEST[$var]))
		$MYREQUEST[$var]=$_REQUEST[$var];
	else
		$MYREQUEST[$var]=$_REQUEST[$var]=NULL;
}

// check parameter sematics
if (empty($MYREQUEST['SCOPE'])) $MYREQUEST['SCOPE']="A";
if (empty($MYREQUEST['SORT1'])) $MYREQUEST['SORT1']="H";
if (empty($MYREQUEST['SORT2'])) $MYREQUEST['SORT2']="D";
if (empty($MYREQUEST['OB']))	$MYREQUEST['OB']=OB_HOST_STATS;
if (!isset($MYREQUEST['COUNT'])) $MYREQUEST['COUNT']=10;
if (!isset($scope_list[$MYREQUEST['SCOPE']])) $MYREQUEST['SCOPE']='A';

$MY_SELF=
	"$PHP_SELF".
	"?SCOPE=".$MYREQUEST['SCOPE'].
	"&SORT1=".$MYREQUEST['SORT1'].
	"&SORT2=".$MYREQUEST['SORT2'].
	"&COUNT=".$MYREQUEST['COUNT'];
$MY_SELF_WO_SORT=
	"$PHP_SELF".
	"?SCOPE=".$MYREQUEST['SCOPE'].
	"&COUNT=".$MYREQUEST['COUNT'];
	
// select cache mode
if ($MYREQUEST['OB'] == OB_USER_CACHE)
	$cache_mode='user';

if(!$cache=@apc_cache_info($cache_mode)) {
	echo "No cache info available.  APC does not appear to be running.";
	exit;
} 
$mem=apc_sma_info();

// don't cache this page
//
header("Cache-Control: no-store, no-cache, must-revalidate");  // HTTP/1.1
header("Cache-Control: post-check=0, pre-check=0", false);
header("Pragma: no-cache");       			                   // HTTP/1.0

// create graphics
//
function graphics_avail() {
	return extension_loaded('gd');
}
if (isset($MYREQUEST['IMG']))
{
	if (!graphics_avail()) {
		exit(0);
	}

	function fill_arc($im, $centerX, $centerY, $diameter, $start, $end, $color1,$color2,$text='') {
		$r=$diameter/2;
		$w=deg2rad((360+$start+($end-$start)/2)%360);
		
		if (function_exists("imagefilledarc")) {
			// exists only if GD 2.0.1 is avaliable
			imagefilledarc($im, $centerX, $centerY, $diameter, $diameter, $start, $end, $color2, IMG_ARC_PIE);
			imagefilledarc($im, $centerX, $centerY, $diameter, $diameter, $start, $end, $color1, IMG_ARC_NOFILL|IMG_ARC_EDGED);
		} else {
			imagearc($im, $centerX, $centerY, $diameter, $diameter, $start, $end, $color2);
			imageline($im, $centerX, $centerY, $centerX + cos(deg2rad($start)) * $r, $centerY + sin(deg2rad($start)) * $r, $color2);
			imageline($im, $centerX, $centerY, $centerX + cos(deg2rad($start+1)) * $r, $centerY + sin(deg2rad($start)) * $r, $color2);
			imageline($im, $centerX, $centerY, $centerX + cos(deg2rad($end-1))   * $r, $centerY + sin(deg2rad($end))   * $r, $color2);
			imageline($im, $centerX, $centerY, $centerX + cos(deg2rad($end))   * $r, $centerY + sin(deg2rad($end))   * $r, $color2);
			imagefill($im,$centerX + $r*cos($w)/2, $centerY + $r*sin($w)/2, $color2);
		}
		if ($text) {
			imagestring($im,4,$centerX + $r*cos($w)/2, $centerY + $r*sin($w)/2,$text,$color1);
		}
	} 
	
	function fill_box($im, $x, $y, $w, $h, $color1, $color2,$text='') {
		$x1=$x+$w-1;
		$y1=$y+$h-1;

		imagefilledrectangle($im, $x, $y1, $x1, $y, $color2);
		if ($text) {
			imagestring($im,4,$x+5,$y1-16,$text,$color1);
		}
	}

	$size = 200; // image size

	$image     = imagecreate($size+50, $size+10);
	$col_white = imagecolorallocate($image, 0xFF, 0xFF, 0xFF);
	$col_red   = imagecolorallocate($image, 0xD0,  0x60,  0x30);
	$col_green = imagecolorallocate($image, 0x60, 0xF0, 0x60);
	$col_black = imagecolorallocate($image,   0,   0,   0);
	imagecolortransparent($image,$col_white);

	if ($MYREQUEST['IMG']==1) {
		$s=$mem['num_seg']*$mem['seg_size'];
		$a=$mem['avail_mem'];
		$x=$y=$size/2;
		$fuzz = 0.000001;

		// This block of code creates the pie chart.  It is a lot more complex than you
		// would expect because we try to visualize any memory fragmentation as well.
		$angle_from = 0;	
		for($i=0; $i<$mem['num_seg']; $i++) {	
			$ptr = 0;
			$free = $mem['block_lists'][$i];
			foreach($free as $block) {
				if($block['offset']!=$ptr) {       // Used block
					$angle_to = $angle_from+($block['offset']-$ptr)/$s;
					if(($angle_to+$fuzz)>1) $angle_to = 1;
					fill_arc($image,$x,$y,$size,$angle_from*360,$angle_to*360,$col_black,$col_red,(($angle_to-$angle_from)>0.05)?bsize($s*($angle_to-$angle_from)):'');
					$angle_from = $angle_to;
				}
				$angle_to = $angle_from+($block['size'])/$s;
				if(($angle_to+$fuzz)>1) $angle_to = 1;
				fill_arc($image,$x,$y,$size,$angle_from*360,$angle_to*360,$col_black,$col_green,(($angle_to-$angle_from)>0.05)?bsize($s*($angle_to-$angle_from)):'');
				$angle_from = $angle_to;
				$ptr = $block['offset']+$block['size'];
			}
		}
	} else {
		$s=$cache['num_hits']+$cache['num_misses'];
		$a=$cache['num_hits'];
		
		fill_box($image, 30,$size,50,-$a*($size-21)/$s,$col_black,$col_green,sprintf("%.1f%%",$cache['num_hits']*100/$s));
		fill_box($image,130,$size,50,-max(4,($s-$a)*($size-21)/$s),$col_black,$col_red,sprintf("%.1f%%",$cache['num_misses']*100/$s));
	}
	header("Content-type: image/png");
	imagepng($image);
	exit;
}

// pretty printer for byte values
//
function bsize($s) {
	foreach (array('','K','M','G') as $i => $k) {
		if ($s < 1024) break;
		$s/=1024;
	}
	return sprintf("%.1f %sBytes",$s,$k);
}

// sortable table header in "scripts for this host" view
function sortheader($key,$name,$extra='') {
	global $MYREQUEST, $MY_SELF_WO_SORT;
	
	if ($MYREQUEST['SORT1']==$key) {
		$MYREQUEST['SORT2'] = $MYREQUEST['SORT2']=='A' ? 'D' : 'A';
	}
	return "<a class=sortable href=\"$MY_SELF_WO_SORT$extra&SORT1=$key&SORT2=".$MYREQUEST['SORT2']."\">$name</a>";

}

// create menu entry 
function menu_entry($ob,$title) {
	global $MYREQUEST,$MY_SELF;
	if ($MYREQUEST['OB']!=$ob) {
		return "<li><a href=\"$MY_SELF&OB=$ob\">$title</a></li>";
	} else if (empty($MYREQUEST['SH'])) {
		return "<li><span class=active>$title</span></li>";
	} else {
		return "<li><a class=\"child_active\" href=\"$MY_SELF&OB=$ob\">$title</a></li>";	
	}
}


?>
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html>
<head><title>APC INFO</title>
<style><!--
body { background:white; font-size:100.01%; margin:0; padding:0; }
body,p,td,th,input,submit { font-size:0.8em;font-family:arial,helvetica,sans-serif; }
* html body   {font-size:0.8em}
* html p      {font-size:0.8em}
* html td     {font-size:0.8em}
* html th     {font-size:0.8em}
* html input  {font-size:0.8em}
* html submit {font-size:0.8em}
td { vertical-align:top }
a { color:black; font-weight:none; text-decoration:none; }
a:hover { text-decoration:underline; }
div.content { padding:1em 1em 1em 1em; position:absolute; width:97%; z-index:100; }
h1.apc { background:rgb(153,153,204);; margin:0; padding:0.5em 1em 0.5em 1em; }
* html h1.apc { margin-bottom:-7px; }
h1.apc a:hover { text-decoration:none; color:rgb(90,90,90); }
h1.apc div.logo span.logo {
	background:rgb(119,123,180); #white;
	color:black; #rgb(153,153,204);
	border-right: solid black 1px;
	border-bottom: solid black 1px;
	font-style:italic;
	font-size:1em;
	padding-left:1.2em;
	padding-right:1.2em;
	text-align:right;
	}
h1.apc div.logo span.name { color:white; font-size:0.7em; padding:0 0.8em 0 2em; }
h1.apc div.nameinfo { color:white; display:inline; font-size:0.4em; margin-left: 3em; }
h1.apc div.copy { color:black; font-size:0.4em; position:absolute; right:1em; }
hr.apc {
	background:white;
	border-bottom:solid rgb(102,102,153) 1px;
	border-style:none;
	border-top:solid rgb(102,102,153) 10px;
	height:12px;
	margin:0;
	margin-top:1px;
	padding:0;
}

ol,menu { margin:1em 0 0 0; padding:0.2em; margin-left:1em;}
ol.menu li { display:inline; margin-right:0.7em; list-style:none}
ol.menu a {
	background:rgb(153,153,204);
	border:solid rgb(102,102,153) 2px;
	color:white;
	font-weight:bold;
	margin-right:0em;
	padding:0.1em 0.5em 0.1em 0.5em;
	text-decoration:none;
	margin-left: 5px;
	}
ol.menu a.child_active {
	background:rgb(153,153,204);
	border:solid rgb(102,102,153) 2px;
	color:white;
	font-weight:bold;
	margin-right:0em;
	padding:0.1em 0.5em 0.1em 0.5em;
	text-decoration:none;
	border-left: solid black 5px;
	margin-left: 0px;
	}
ol.menu span.active {
	background:rgb(153,153,204);
	border:solid rgb(102,102,153) 2px;
	color:black;
	font-weight:bold;
	margin-right:0em;
	padding:0.1em 0.5em 0.1em 0.5em;
	text-decoration:none;
	border-left: solid black 5px;
	}
ol.menu a:hover { background:rgb(193,193,244);}
div.info {
	background:rgb(204,204,204);
	border:solid rgb(204,204,204) 1px;
	margin-bottom:1em;
	}
div.info h2 {
	background:rgb(204,204,204);
	color:black;
	font-size:1em;
	margin:0;
	padding:0.1em 1em 0.1em 1em;
	}
div.info table {
	border:solid rgb(204,204,204) 1px;
	border-spacing:0;
	width:100%;
	}
div.info table th {
	background:rgb(204,204,204);
	color:white;
	margin:0;
	padding:0.1em 1em 0.1em 1em;
	}
div.info table th a.sortable { color:black; }
div.info table tr.tr-0 { background:rgb(238,238,238); }
div.info table tr.tr-1 { background:rgb(221,221,221); }
div.info table td { padding:0.3em 1em 0.3em 1em; }
div.info table td.td-0 { border-right:solid rgb(102,102,153) 1px; white-space:nowrap; }
div.info table td.td-n { border-right:solid rgb(102,102,153) 1px; }
div.info table td h3 {
	color:black;
	font-size:1.1em;
	margin-left:-0.3em;
	}

div.graph { background:rgb(204,204,204); border:solid rgb(204,204,204) 1px; margin-bottom:1em }
div.graph h2 { background:rgb(204,204,204);; color:black; font-size:1em; margin:0; padding:0.1em 1em 0.1em 1em; }
div.graph table { border:solid rgb(204,204,204) 1px; color:black; font-weight:normal; width:100%; }
div.graph table td.td-0 { background:rgb(238,238,238); }
div.graph table td.td-1 { background:rgb(221,221,221); }
div.graph table td { padding:0.2em 1em 0.2em 1em; }

div.div1,div.div2 { margin-bottom:1em; width:35em; }
div.div3 { position:absolute; left:37em; top:1em; right:1em; }

div.sorting { margin:1.5em 0em 2em 2em }
.center { text-align:center }
.right { position:absolute;right:1em }
.ok { color:rgb(0,200,0); font-weight:bold}
.failed { color:rgb(200,0,0); font-weight:bold}
input {
	background:rgb(153,153,204);
	border:solid rgb(102,102,153) 2px;
	color:white;
	font-weight:bold;
	margin-right:1em;
	padding:0.1em 0.5em 0.1em 0.5em;
	}
//-->
</style>
</head>
<body>
<h1 class=apc><div class=logo><span class=logo><a href="http://pecl.php.net/package/APC">APC</a></span></div>
<div class=nameinfo>Opcode Cache</div>
</div></h1>
<hr class=apc>
<?php

// clear clear cache need's ADMIN_PASSWORD to be changed!
//
if (isset($MYREQUEST['CC']) && $MYREQUEST['CC']) {
	if(ADMIN_PASSWORD!='' && ADMIN_PASSWORD!='password') 
		apc_clear_cache();
}
if(ADMIN_PASSWORD=='' || ADMIN_PASSWORD=='password')
	$sure_msg = "alert('You need to set a password at the top of apc.php before this will work')";
else
	$sure_msg = "confirm('Are you sure?');";


// Display main Menu
echo <<<EOB
	<ol class=menu>
	<li><a href="$MY_SELF&OB=$OB">Refresh Data</a></li>
EOB;
echo
	menu_entry(1,'View Host Stats'),
	menu_entry(2,'System Cache Entries'),
	menu_entry(3,'User Cache Entries'),
	menu_entry(9,'Version Check');
echo <<<EOB
	<li><a class="right" href="$MY_SELF&CC=1&OB=$OB" onClick="javascipt:return $sure_msg;">Clear Cache</a></li>
	</ol>
EOB;



// MAIN SWITCH STATEMENT 

switch ($MYREQUEST['OB']) {





// -----------------------------------------------
// Host Stats
// -----------------------------------------------
case OB_HOST_STATS:
	$mem_size = $mem['num_seg']*$mem['seg_size'];
	$mem_avail= $mem['avail_mem'];
	$mem_used = $mem_size-$mem_avail;
	$seg_size = bsize($mem['seg_size']);
	$req_rate = sprintf("%.2f",($cache['num_hits']+$cache['num_misses'])/($time-$cache['start_time']));
	$apcversion = phpversion('apc');
	$phpversion = phpversion();
	$number_cached = count($cache['cache_list']);
	$i=0;
	echo <<< EOB
		<div class=content>		
		<div class="info div1"><h2>General Cache Information</h2>
		<table cellspacing=0><tbody>
		<tr class=tr-0><td class=td-0>APC Version</td><td>$apcversion</td></tr>
		<tr class=tr-1><td class=td-0>PHP Version</td><td>$phpversion</td></tr>
EOB;

	if(!empty($_SERVER['SERVER_NAME']))
		echo "<tr class=tr-0><td class=td-0>APC Host</td><td>{$_SERVER['SERVER_NAME']}</td></tr>\n";
	if(!empty($_SERVER['SERVER_SOFTWARE']))
		echo "<tr class=tr-1><td class=td-0>Server Software</td><td>{$_SERVER['SERVER_SOFTWARE']}</td></tr>\n";

	echo <<<EOB
		<tr class=tr-0><td class=td-0>Cached Files</td><td>$number_cached</td></tr>
		<tr class=tr-1><td class=td-0>Hits</td><td>{$cache['num_hits']}</td></tr>
		<tr class=tr-0><td class=td-0>Misses</td><td>{$cache['num_misses']}</td></tr>
		<tr class=tr-1><td class=td-0>Request Rate</td><td>$req_rate requests/second</td></tr>
		<tr class=tr-0><td class=td-0>Shared Memory</td><td>{$mem['num_seg']} Segment(s) with $seg_size</td></tr>
		</tbody></table>
		</div>

		<div class="info div2"><h2>Runtime Settings</h2><table cellspacing=0><tbody>
EOB;

	$j = 0;
	foreach (ini_get_all('apc') as $k => $v) {
		echo "<tr class=tr-$j><td class=td-0>",$k,"</td><td>",$v['local_value'],"</td></tr>\n";
		$j = 1 - $j;
	}

	if($mem['num_seg']>1 || $mem['num_seg']==1 && count($mem['block_lists'][0])>1)
		$mem_note = "Memory Usage<br /><font size=-2>(multiple slices indicate fragments)</font>";
	else
		$mem_note = "Memory Usage";

	echo <<< EOB
		</tbody></table>
		</div>
		
		<div class="graph div3"><h2>Host Status Diagrams</h2>
		<table cellspacing=0><tbody>
		<tr>
		<td class=td-0>$mem_note</td>
		<td class=td-1>Hits &amp; Misses</td>
		</tr>
EOB;
	echo
		graphics_avail() ? 
			  "<tr><td class=td-0><img alt=\"\" src=\"$PHP_SELF?IMG=1&$time\"></td><td class=td-1><img alt=\"\" src=\"$PHP_SELF?IMG=2&$time\"></td></tr>\n"
			: "",
		"<tr>\n",
		"<td class=td-0><font style=\"background:#60F060; border: #000000 solid 1px;\"> &nbsp; &nbsp; </font> &nbsp; Free: ",bsize($mem_avail).sprintf(" (%.1f%%)",$mem_avail*100/$mem_size),"</td>\n",
		"<td class=td-1><font style=\"background:#60F060; border: #000000 solid 1px;\"> &nbsp; &nbsp; </font> &nbsp; Hits: ",$cache['num_hits'].sprintf(" (%.1f%%)",$cache['num_hits']*100/($cache['num_hits']+$cache['num_misses'])),"</td>\n",
		"</tr>\n",
		"<tr>\n",
		"<td class=td-0><font style=\"background:#D06030; border: #000000 solid 1px;\"> &nbsp; &nbsp; </font> &nbsp; Used: ",bsize($mem_used ).sprintf(" (%.1f%%)",$mem_used *100/$mem_size),"</td>\n",
		"<td class=td-1><font style=\"background:#D06030; border: #000000 solid 1px;\"> &nbsp; &nbsp; </font> &nbsp; Misses: ",$cache['num_misses'].sprintf(" (%.1f%%)",$cache['num_misses']*100/($cache['num_hits']+$cache['num_misses'])),"</td>\n";
	echo <<< EOB
		</tr>
		</tbody></table>
		</div>

		</div>
EOB;
		
	break;




// -----------------------------------------------
// System Cache Entries		
// -----------------------------------------------
case OB_SYS_CACHE:	
	$fieldname='filename';
	$fieldheading='Script Filename';
	$fieldkey='inode';
	
// -----------------------------------------------
 // User Cache Entries
// -----------------------------------------------
case OB_USER_CACHE:
	if (!isset($fieldname))
	{
		$fieldname='info';
		$fieldheading='User Entry Label';
		$fieldkey='info';
	}

	$OB=$MYREQUEST['OB'];

	if (!empty($MYREQUEST['SH']))
	{
		echo <<< EOB
			<div class=content>

			<div class="info"><table cellspacing=0><tbody>
			<tr><th>Attribute</th><th>Value</th></tr>
EOB;

		$m=0;
		foreach($scope_list as $j => $list) {
			foreach($cache[$list] as $i => $entry) {
				if (md5($entry[$fieldkey])!=$MYREQUEST['SH']) continue;
				foreach($entry as $k => $value) {
					if ($k == "num_hits") {
						$value=sprintf("%s (%.2f%%)",$value,$value*100/$cache['num_hits']);
					}
					if ($k == 'deletion_time') {
						if(!$entry['deletion_time']) $value = "None";
					}
					echo
						"<tr class=tr-$m>",
						"<td class=td-0>",ucwords(preg_replace("/_/"," ",$k)),"</td>",
						"<td class=td-last>",(preg_match("/time/",$k) && $value!='None') ? date(DATE_FORMAT,$value) : $value,"</td>",
						"</tr>";
					$m=1-$m;
				}
				if($fieldkey=='info') {
					if(ADMIN_PASSWORD!='password') {
						echo "<tr class=tr-$m><td class=td-0>Stored Value</td><td class=td-last><pre>";
						$output = var_export(apc_fetch($entry[$fieldkey]),true);
						echo htmlspecialchars($output);
						echo "</pre></td></tr>\n";
					} else {
						echo
						"<tr class=tr-$m>",
						"<td class=td-0>Stored Value</td>",
						"<td class=td-last>Set your apc.php password to see the user values here</td>",
						"</tr>\n";
					}
				}
				break 2;
			}
		}

		echo
			"</tbody></table>\n",
			"</div>",

			"</div>";
		break;
	}

	$cols=5;
	echo <<<EOB
		<div class=sorting><form>Scope:
		<input type=hidden name=OB value=$OB>
		<select name=SCOPE>
EOB;
	echo 
		"<option value=A",$MYREQUEST['SCOPE']=='A' ? " selected":"",">Active</option>",
		"<option value=D",$MYREQUEST['SCOPE']=='D' ? " selected":"",">Deleted</option>",
		"</select>",
		", Sorting:<select name=SORT1>",
		"<option value=H",$MYREQUEST['SORT1']=='H' ? " selected":"",">Hits</option>",
		"<option value=S",$MYREQUEST['SORT1']=='S' ? " selected":"",">$fieldheading</option>",
		"<option value=M",$MYREQUEST['SORT1']=='M' ? " selected":"",">Last modified</option>",
		"<option value=C",$MYREQUEST['SORT1']=='C' ? " selected":"",">Created at</option>",
		"<option value=D",$MYREQUEST['SORT1']=='D' ? " selected":"",">Deleted at</option>";
	if($fieldname=='info') echo
		"<option value=D",$MYREQUEST['SORT1']=='T' ? " selected":"",">Timeout</option>";
	echo 
		'</select>',
		'<select name=SORT2>',
		'<option value=D',$MYREQUEST['SORT2']=='D' ? ' selected':'','>DESC</option>',
		'<option value=A',$MYREQUEST['SORT2']=='A' ? ' selected':'','>ASC</option>',
		'</select>',
		'<select name=COUNT onChange="form.submit()">',
		'<option value=10 ',$MYREQUEST['COUNT']=='10' ? ' selected':'','>Top 10</option>',
		'<option value=20 ',$MYREQUEST['COUNT']=='20' ? ' selected':'','>Top 20</option>',
		'<option value=50 ',$MYREQUEST['COUNT']=='50' ? ' selected':'','>Top 50</option>',
		'<option value=100',$MYREQUEST['COUNT']=='100'? ' selected':'','>Top 100</option>',
		'<option value=150',$MYREQUEST['COUNT']=='150'? ' selected':'','>Top 150</option>',
		'<option value=200',$MYREQUEST['COUNT']=='200'? ' selected':'','>Top 200</option>',
		'<option value=500',$MYREQUEST['COUNT']=='500'? ' selected':'','>Top 500</option>',
		'<option value=-1', $MYREQUEST['COUNT']=='0'  ? ' selected':'','>All</option>',
		'</select>',
		'&nbsp;<input type=submit value="GO!">',
		'</form></div>',

		'<div class=content>',

		'<div class="info"><table cellspacing=0><tbody>',
		'<tr>',
		'<th>',sortheader('S',$fieldheading,"&OB=$OB"),  '</th>',
		'<th>',sortheader('H','Hits',"&OB=$OB"),         '</th>',
		'<th>',sortheader('M','Last modified',"&OB=$OB"),'</th>',
		'<th>',sortheader('C','Created at',"&OB=$OB"),   '</th>';

	if($fieldname=='info') {
		$cols++;
		 echo '<th>',sortheader('T','Timeout',"&OB=$OB"),'</th>';
	}
	echo '<th>',sortheader('D','Deleted at',"&OB=$OB"),'</th></tr>';

	// buils list with alpha numeric sortable keys
	//
	foreach($cache[$scope_list[$MYREQUEST['SCOPE']]] as $i => $entry) {
		switch($MYREQUEST['SORT1']) {
			case "H": $k=sprintf("%015d-",$entry['num_hits']); 		break;
			case "M": $k=sprintf("%015d-",$entry['mtime']);			break;
			case "C": $k=sprintf("%015d-",$entry['creation_time']);	break;
			case "T": $k=sprintf("%015d-",$entry['ttl']);			break;
			case "D": $k=sprintf("%015d-",$entry['deletion_time']);	break;
			case "S": $k='';										break;
		}
		$list[$k.$entry['filename']]=$entry;
	}
	if (isset($list) && is_array($list)) {
		
		// sort list
		//
		switch ($MYREQUEST['SORT2']) {
			case "A":	krsort($list);	break;
			case "D":	ksort($list);	break;
		}
		
		// output list
		$i=0;
		foreach($list as $k => $entry) {
			echo
				'<tr class=tr-',$i%2,'>',
				"<td class=td-0><a href=\"$MY_SELF&OB=$OB&SH=",md5($entry[$fieldkey]),"\">",$entry[$fieldname],'</a></td>',
				'<td class="td-n center">',$entry['num_hits'],'</td>',
				'<td class="td-n center">',date(DATE_FORMAT,$entry['mtime']),'</td>',
				'<td class="td-n center">',date(DATE_FORMAT,$entry['creation_time']),'</td>';

			if($fieldname=='info') {
				if($entry['ttl'])
					echo '<td class="td-n center">'.$entry['ttl'].' seconds</td>';
				else
					echo '<td class="td-n center">None</td>';
			}
			echo
				'<td class="td-last center">',$entry['deletion_time'] ? date(DATE_FORMAT,$entry['deletion_time']) : '-','</td>',
				'</tr>';
			$i++;
			if ($i == $MYREQUEST['COUNT'])
				break;
		}
		
	} else {
		echo '<tr class=tr-0><td class="center" colspan=',$cols,'><i>No data</i></td></tr>';
	}
	echo <<< EOB
		</tbody></table>
EOB;

	if ($i < count($list)) {
		echo "<a href=\"$MY_SELF&OB=$OB&COUNT=0\"><i>",count($list)-$i,' more available...</i></a>';
	}

	echo <<< EOB
		</div>
		</div>
EOB;
	break;



		
// -----------------------------------------------
// Version check
// -----------------------------------------------
case OB_VERSION_CHECK:
	echo <<<EOB
		<div class=content>
		
		<div class="info"><h2>APC Version Information</h2>
		<table cellspacing=0><tbody>
		<tr>
		<th></th>
		</tr>
EOB;

	$rss = @file_get_contents("http://pecl.php.net/feeds/pkg_apc.rss");
	if (!$rss) {
		echo '<tr class="td-last center"><td>Unable to fetch version information.</td></tr>';
	} else {
		$apcversion = phpversion('apc');

		preg_match('!<title>APC ([0-9.]+)</title>!', $rss, $match);
		echo '<tr class="tr-0 center"><td>';
		if (version_compare($apcversion, $match[1], '>=')) {
			echo '<div class="ok">You are running the latest version of APC ('.$apcversion.')</div>';
			$i = 3;
		} else {
			echo '<div class="failed">You are running an older version of APC ('.$apcversion.'), 
				newer version '.$match[1].' is available at <a href="http://pecl.php.net/package/APC/'.$match[1].'">
				http://pecl.php.net/package/APC/'.$match[1].'</a>
				</div>';
			$i = -1;
		}
		echo '</td></tr>';
		echo '<tr class="tr-0"><td><h3>Change Log:</h3><br/>';

		preg_match_all('!<(title|description)>([^<]+)</\\1>!', $rss, $match);
		next($match[2]); next($match[2]);

		while (list(,$v) = each($match[2])) {
			list(,$ver) = explode(' ', $v, 2);
			if ($i < 0 && version_compare($apcversion, $ver, '>=')) {
				break;
			} else if (!$i--) {
				break;
			}
			echo "<b><a href=\"http://pecl.php.net/package/APC/$ver\">".htmlspecialchars($v)."</a></b><br><blockquote>";
			echo nl2br(htmlspecialchars(current($match[2])))."</blockquote>";
			next($match[2]);
		}
		echo '</td></tr>';
	}
	echo <<< EOB
		</tbody></table>
		</div>
		
		</div>
EOB;
	break;



}
?>

<!-- <?php echo "\nBased on APCGUI By R.Becker\n$VERSION\n"?> -->
</body>
</html>
