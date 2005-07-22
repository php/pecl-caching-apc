<?php
$VERSION='$Id$';

$SKIN='pecl'; // ('pecl' or 'classic')
$admin_password = 'password';  // Change this to enable the Clear Cache Command

// rewrite $PHP_SELF to block XSS attacks
$PHP_SELF= isset($_SERVER['PHP_SELF']) ? htmlentities(strip_tags($_SERVER['PHP_SELF'],'')) : '';
$time = time();

// check validity of input variables
$vardom=array(
	'CC'	=> '/^[01]$/',
	'COUNT'	=> '/^\d+$/',
	'IMG'	=> '/^[12]$/',
	'OB'	=> '/^[01]$/',
	'SCOPE'	=> '/^[AD]$/',
	'SH'	=> '/^\d+$/',
	'SORT1'	=> '/^[HSMCD]$/',
	'SORT2'	=> '/^[DA]$/',
);

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

foreach($vardom as $var => $dom)
{
	if (!isset($_REQUEST[$var])) {
		$MYREQUEST[$var]=NULL;
		continue;
	} if (!is_array($_REQUEST[$var]) && preg_match($dom,$_REQUEST[$var]))
		$MYREQUEST[$var]=$_REQUEST[$var];
	else
		$MYREQUEST[$var]=$_REQUEST[$var]=NULL;
}

// don't cache this page
//
header("Cache-Control: no-store, no-cache, must-revalidate");  // HTTP/1.1
header("Cache-Control: post-check=0, pre-check=0", false);
header("Pragma: no-cache");                          // HTTP/1.0

// create graphics
//
function graphics_avail()
{
	return extension_loaded('gd');
}
if (isset($MYREQUEST['IMG']))
{
	if (!graphics_avail()) {
		exit(0);
	}

	function fill_arc($im, $centerX, $centerY, $diameter, $start, $end, $color1,$color2,$text='')
	{
		$r=$diameter/2;
		$w=deg2rad((360+$start+($end-$start)/2)%360);
		
		imagefilledarc($im, $centerX, $centerY, $diameter, $diameter, $start, $end, $color2, IMG_ARC_EDGED);
/*
		imageline($im, $centerX, $centerY, $centerX + cos(deg2rad($start)) * $r, $centerY + sin(deg2rad($start)) * $r, $color2);
		imageline($im, $centerX, $centerY, $centerX + cos(deg2rad($start+1)) * $r, $centerY + sin(deg2rad($start)) * $r, $color2);
		imageline($im, $centerX, $centerY, $centerX + cos(deg2rad($end-1))   * $r, $centerY + sin(deg2rad($end))   * $r, $color2);
		imageline($im, $centerX, $centerY, $centerX + cos(deg2rad($end))   * $r, $centerY + sin(deg2rad($end))   * $r, $color2);
		imagefill($im,$centerX + $r*cos($w)/2, $centerY + $r*sin($w)/2, $color2);
*/
		
		if ($text)
			imagestring($im,4,$centerX + $r*cos($w)/2, $centerY + $r*sin($w)/2,$text,$color1);
	} 
	
	function fill_box($im, $x, $y, $w, $h, $color1, $color2,$text='')
	{
		$x1=$x+$w-1;
		$y1=$y+$h-1;

/* Probably want an imagefilledrectangle call here instead
		imagefilledrectangle($im, $x, $y, $x1, $y1, $color2);
*/
		imageline($im, $x, $y, $x1,$y,  $color2);
		imageline($im, $x1,$y, $x1,$y1, $color2);
		imageline($im, $x1,$y1,$x, $y1, $color2);
		imageline($im, $x, $y1,$x, $y,  $color2);
		imagefill ($im,$x+$w/2,$y+$h/2,$color2);
		if ($text)
			imagestring($im,4,$x+5,$y1-16,$text,$color1);
	}

	$size=200;

	$image = imagecreate($size+10, $size+10);
	$col_white = imagecolorallocate($image, 255, 255, 255);
	$col_red   = imagecolorallocate($image, 200,  80,  30);
	$col_green = imagecolorallocate($image, 100, 255, 100);
	$col_black = imagecolorallocate($image,   0,   0,   0);
	imagecolortransparent($image,$col_white);

	if ($MYREQUEST['IMG']==1)
	{
		$mem=apc_sma_info();
		$s=$mem['num_seg']*$mem['seg_size'];
		$a=$mem['avail_mem'];

		$x=$y=$size/2;

		fill_arc($image,$x,$y,$size,0,$a*360/$s,$col_black,$col_green,bsize($a));
		fill_arc($image,$x,$y,$size,0+$a*360/$s,360,$col_black,$col_red,bsize($s-$a));
	}
	else
	{
		$cache=apc_cache_info();
		$s=$cache['num_hits']+$cache['num_misses'];
		$a=$cache['num_hits'];
		
		fill_box($image, 30,$size,50,-$a*($size-21)/$s,$col_black,$col_green,sprintf("%.1f%%",$cache['num_hits']*100/$s));
		fill_box($image,130,$size,50,-max(4,($s-$a)*($size-21)/$s),$col_black,$col_red,sprintf("%.1f%%",$cache['num_misses']*100/$s));
	}
	header("Content-type: image/png");
	imagepng($image);
	exit(0);
}

// pretty printer for byte values
//
function bsize($s)
{
	foreach (array('','K','M','G') as $i => $k)
	{
		if ($s < 1024) break;
		$s/=1024;
	}
	return sprintf("%.1f %sBytes",$s,$k);
}

// sortaable table header in "scripts for this host" view
function sortheader($key,$name,$extra='')
{
	global $MYREQUEST, $MY_SELF_WO_SORT;
	
	if ($MYREQUEST['SORT1']==$key)
		$MYREQUEST['SORT2'] = $MYREQUEST['SORT2']=='A' ? 'D' : 'A';

	return "<a class=sortable href=\"$MY_SELF_WO_SORT$extra&SORT1=$key&SORT2=".$MYREQUEST['SORT2']."\">$name</a>";

}

?>
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html>
<head><title>APC PHP/GUI</title>
<link rel="stylesheet" href="./apcgui-<?php echo $SKIN?>.css">
<head>
<body>
<h1 class=apc><div class=logo><span class=logo>APC</span> <span class=name>{ PHP/GUI }</span></div>
<div class=nameinfo>Alternative PHP Cache</div>
<div class=copy>&copy; 2005 <a href=mailto:beckerr@fh-trier.de>R.Becker</a></div></h1>
<hr class=apc>
<?php

$scope_list=array(
	'A' => 'cache_list',
	'D' => 'deleted_list'
);

if (isset($MYREQUEST['CC']) && $MYREQUEST['CC'])
{
	global $admin_password;

	if($admin_password && $admin_password!='password') 
		apc_clear_cache();
}


if (!isset($MYREQUEST['SCOPE'])) $MYREQUEST['SCOPE']="A";
if (!isset($MYREQUEST['SORT1'])) $MYREQUEST['SORT1']="H";
if (!isset($MYREQUEST['SORT2'])) $MYREQUEST['SORT2']="D";
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

$cache=apc_cache_info();

if(!admin_password || $admin_password=='password')
	$sure_msg = "You need to set a password at the top of apcgui.php before this will work";
else
	$sure_msg = "Are you sure?";

if (isset($MYREQUEST['SH']) && $MYREQUEST['SH'])
{
	echo
		"<ol class=menu>",
		"<li><a href=\"$MY_SELF&OB=0\">View host stats</a></li>",
		"<li><a href=\"$MY_SELF&OB=1\">Scripts for this host</a></li>",
		"</ol>\n",
		"<div class=content>\n",
		
		"<div class=\"info\"><table cellspacing=0><tbody>",
		"<tr><th>Attribute</th><th>Value</th></tr>";

	$m=0;
	foreach($scope_list as $j => $list)
		foreach($cache[$list] as $i => $entry)
		{
			if ($entry['inode']!=$MYREQUEST['SH']) continue;
			foreach($entry as $k => $value)
			{
				if ($k == "num_hits")
				{
					$value=sprintf("%s (%.2f%%)",$value,$value*100/$cache['num_hits']);
				}
				echo
					"<tr class=tr-$m>",
					"<td class=td-0>",ucwords(preg_replace("/_/"," ",$k)),"</td>",
					"<td class=td-last>",preg_match("/time/",$k) ? date("d.m.Y H:i:s",$value) : $value,"</td>",
					"</tr>";
				$m=1-$m;
			}
		}

	echo
		"</tbody></table>\n",
		"</div>",
		
		"</div>";
	
}
else
if (isset($MYREQUEST['OB']) && $MYREQUEST['OB'])
{
	echo <<<EOB
		<ol class=menu>
		<li><a href="$MY_SELF&OB=1">Refresh Data</a></li>
		<li><a href="$MY_SELF&OB=0">View host stats</a></li>
		<li><a class="right" href="$MY_SELF&CC=1" onClick="javascipt:return confirm('$sure_msg');">Clear Cache</a></li>
		</ol>
		<div class=sorting><form>Scope:
		<input type=hidden name=OB value=1>
		<select name=SCOPE>
EOB;
	echo 
		"<option value=A",$MYREQUEST['SCOPE']=='A' ? " selected":"",">Active</option>",
		"<option value=D",$MYREQUEST['SCOPE']=='D' ? " selected":"",">Deleted</option>",
		"</select>",
		", Sorting:<select name=SORT1>",
		"<option value=H",$MYREQUEST['SORT1']=='H' ? " selected":"",">Hits</option>",
		"<option value=S",$MYREQUEST['SORT1']=='S' ? " selected":"",">Scriptname</option>",
		"<option value=M",$MYREQUEST['SORT1']=='M' ? " selected":"",">Last modified</option>",
		"<option value=C",$MYREQUEST['SORT1']=='C' ? " selected":"",">Created at</option>",
		"<option value=D",$MYREQUEST['SORT1']=='D' ? " selected":"",">Deleted at</option>",
		"</select>",
		"<select name=SORT2>",
		"<option value=D",$MYREQUEST['SORT2']=='D' ? " selected":"",">DESC</option>",
		"<option value=A",$MYREQUEST['SORT2']=='A' ? " selected":"",">ASC</option>",
		"</select>",
		"<select name=COUNT>",
		"<option value=10 ",$MYREQUEST['COUNT']=='10' ? " selected":"",">Top 10</option>",
		"<option value=20 ",$MYREQUEST['COUNT']=='20' ? " selected":"",">Top 20</option>",
		"<option value=50 ",$MYREQUEST['COUNT']=='50' ? " selected":"",">Top 50</option>",
		"<option value=100",$MYREQUEST['COUNT']=='100'? " selected":"",">Top 100</option>",
		"<option value=150",$MYREQUEST['COUNT']=='150'? " selected":"",">Top 150</option>",
		"<option value=200",$MYREQUEST['COUNT']=='200'? " selected":"",">Top 200</option>",
		"<option value=500",$MYREQUEST['COUNT']=='500'? " selected":"",">Top 500</option>",
		"<option value=-1", $MYREQUEST['COUNT']=='-1' ? " selected":"",">All</option>",
		"</select>",
		"&nbsp;<input type=submit value=\"GO!\">",
		"</form></div>",
		
		"<div class=content>\n",
		
		"<div class=\"info\"><table cellspacing=0><tbody>",
		"<tr>",
		"<th>",sortheader('S','Scriptname','&OB=1'),"</th>",
		"<th>",sortheader('H','Hits','&OB=1'),"</th>",
		"<th>",sortheader('M','Last modified','&OB=1'),"</th>",
		"<th>",sortheader('C','Created at','&OB=1'),"</th>",
		"<th>",sortheader('D','Deleted at','&OB=1'),"</th></tr>";

	foreach($cache[$scope_list[$MYREQUEST['SCOPE']]] as $i => $entry)
	{
		switch($MYREQUEST['SORT1'])
		{
			case "H": $k=sprintf("%015d-",$entry['num_hits']); 		break;
			case "M": $k=sprintf("%015d-",$entry['mtime']);			break;
			case "C": $k=sprintf("%015d-",$entry['creation_time']);	break;
			case "D": $k=sprintf("%015d-",$entry['deletion_time']);	break;
			case "S": $k='';										break;
		}
		$list[$k.$entry['filename']]=$entry;
	}
	if (isset($list) && is_array($list))
	{
		function strcmp_desc($a, $b)
		{
			return strcmp($a,$b);
		}
		switch ($MYREQUEST['SORT2'])
		{
			case "A":	krsort($list);	break;
			case "D":	ksort($list);	break;
		}
		$i=0;
		foreach($list as $k => $entry)
		{
			echo
				"<tr class=tr-",$i%2,">",
				"<td class=td-0><a href=\"$MY_SELF&SH=",$entry['inode'],"\">",$entry['filename'],"</a></td>",
				"<td class=\"td-n center\">",$entry['num_hits'],"</td>",
				"<td class=\"td-n center\">",date("d.m.Y H:i:s",$entry['mtime']),"</td>",
				"<td class=\"td-n center\">",date("d.m.Y H:i:s",$entry['creation_time']),"</td>",
				"<td class=\"td-last center\">",$entry['deletion_time'] ? date("d.m.Y H:i:s",$entry['deletion_time']) : "-","</td>",
				"</tr>";
			$i++;
			if (isset($MYREQUEST['COUNT']) && $MYREQUEST['COUNT']!=-1 && $i >= $MYREQUEST['COUNT']) break;
		}
	}
	else
	{
		echo "<tr class=tr-0><td class=\"center\" colspan=5><i>No data</i></td></tr>";
	}
	echo
		"</tbody></table>\n",
		"</div>",
		
		"</div>";
}
else
{
	$mem=apc_sma_info();
	$mem_size = $mem['num_seg']*$mem['seg_size'];
	$mem_avail= $mem['avail_mem'];
	$mem_used = $mem_size-$mem_avail;
	$seg_size = bsize($mem['seg_size']);
	$req_rate = sprintf("%.2f",($cache['num_hits']+$cache['num_misses'])/($time-$cache['start_time']));
	$apcversion = phpversion('apc');
	$phpversion = phpversion();
	echo <<< EOB
		<ol class=menu>
		<li><a href="$MY_SELF&OB=0">Refresh Data</a></li>
		<li><a href="$MY_SELF&OB=1">Scripts for this host</a></li>
		<li><a class="right" href="$MY_SELF&CC=1" onClick="javascipt:return confirm('$sure_msg');">Clear Cache</a></li>
		</ol>
		<div class=content>
		
		<div class="info div1"><h2>General Cache Information</h2>
		<table cellspacing=0><tbody>
		<tr class=tr-0><td class=td-0>APC Version</td><td>$apcversion</td></tr>
		<tr class=tr-1><td class=td-0>PHP Version</td><td>$phpversion</td></tr>
EOB;

	if(!empty($_SERVER['SERVER_NAME']))
		echo "<tr class=tr-0><td class=td-0>APC Host</td><td>{$_SERVER['SERVER_NAME']}</td></tr>\n";
	if(!empty($_SERVER['$SERVER_SOFTWARE'])
		echo "<tr class=tr-1><td class=td-0>Server Software</td><td>{$_SERVER['SERVER_SOFTWARE']}</td></tr>\n";

	echo <<<EOB
		<tr class=tr-0><td class=td-0>Hits</td><td>{$cache['num_hits']}</td></tr>
		<tr class=tr-1><td class=td-0>Misses</td><td>{$cache['num_misses']}</td></tr>
		<tr class=tr-0><td class=td-0>Request Rate</td><td>$req_rate requests/second</td></tr>
		<tr class=tr-1><td class=td-0>Shared Memory</td><td>{$mem['num_seg']} Segment(s) with $seg_size</td></tr>
		</tbody></table>
		</div>

		<div class="info div2"><h2>Runtime Settings</h2><table cellspacing=0><tbody>
EOB;

	$j = 0;
	foreach (ini_get_all('apc') as $k => $v) {
		echo "<tr class=tr-$j><td class=td-0>",$k,"</td><td>",$v['local_value'],"</td></tr>\n";
		$j = 1 - $j;
	}

	echo <<< EOB
		</tbody></table>
		</div>
		
		<div class="graph div3"><h2>Hoststatus Diagrams</h2>
		<table cellspacing=0><tbody>
		<tr>
		<td class=td-0>Memory Usage</td>
		<td class=td-1>Hits & Misses</td>
		</tr>
EOB;
	echo
		graphics_avail() ? 
			  "<tr><td class=td-0><img alt=\"\" src=\"$PHP_SELF?IMG=1&$time\"></td><td class=td-1><img alt=\"\" src=\"$PHP_SELF?IMG=2&$time\"></td></tr>\n"
			: "",
		"<tr>\n",
		"<td class=td-0>Free: ",bsize($mem_avail).sprintf(" (%.1f%%)",$mem_avail*100/$mem_size),"</td>\n",
		"<td class=td-1>Hits: ",$cache['num_hits'].sprintf(" (%.1f%%)",$cache['num_hits']*100/($cache['num_hits']+$cache['num_misses'])),"</td>\n",
		"</tr>\n",
		"<tr>\n",
		"<td class=td-0>Used: ",bsize($mem_used ).sprintf(" (%.1f%%)",$mem_used *100/$mem_size),"</td>\n",
		"<td class=td-1>Misses: ",$cache['num_misses'].sprintf(" (%.1f%%)",$cache['num_misses']*100/($cache['num_hits']+$cache['num_misses'])),"</td>\n";
	echo <<< EOB
		</tr>
		</tbody></table>
		</div>

		</div>
EOB;
		
}
?>

<!-- <?php echo "$VERSION"?> -->
</body>
</html>
