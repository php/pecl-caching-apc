<?php
$ff = $saf = $msie = null;
if(stristr(getenv("HTTP_USER_AGENT"),'firefox')) $ff=true;
if(stristr(getenv("HTTP_USER_AGENT"),'safari')) $saf=true;
if(stristr(getenv("HTTP_USER_AGENT"),'msie')) $msie=true;

function redir($arg="") {
	$port = getenv('SERVER_PORT');
	$host = htmlspecialchars(getenv('HTTP_HOST'));
	$self = htmlspecialchars(getenv('PHP_SELF'));
	if($port==443) $url = "https://";
	else $url = "http://";
	$url .= $host;
	if($port!=80) $url .= ":$port";
	$url .= $self;
	if($arg) $url.="?$arg";
	header("Location: $url");
	exit;
}
if(isset($_GET['action'])) {
    switch($_GET['action']) {
      case 'Top 25': 
		$limit=25;
		if(isset($_GET['last_mode']) && $_GET['last_mode']=='user') { $mode='user'; $field = 'info'; }
		else { $mode='opcode'; $field = 'filename'; }
		break;
      case 'Top 100': 
		$limit=100; 
		if(isset($_GET['last_mode']) && $_GET['last_mode']=='user') { $mode='user'; $field = 'info'; }
		else { $mode='opcode'; $field = 'filename'; }
		break;
      case 'All': 
		$limit=0;
		if(isset($_GET['last_mode']) && $_GET['last_mode']=='user') { $mode='user'; $field = 'info'; }
		else { $mode='opcode'; $field = 'filename'; }
		break;
	  case 'User Cache': 
		$limit=25; 
		$mode='user'; $field = 'info';
		break;
	  case 'Opcode Cache':
		$limit=25;
		$mode='opcode'; $field = 'filename';
		break;
    }
} else {
    $limit = 25; 
	$mode = 'opcode';
	$field = 'filename';
}

if(!$cache_info = @apc_cache_info($mode)) {
	echo "No cache info available.  Did you disable it in your apc.ini file?";
	exit;
}
$ttl = $cache_info['ttl'];
$sma_info = apc_sma_info();
if($ff) $bg = "#cccccc"; else $bg = "#ffffff";
if($msie) $right_float_width = "10em";
else $right_float_width = "auto";
if($saf) $left_float_style = "display: table; width: 80%;";
else $left_float_style = "";

// Colours: #9999cc, #ccccff and #cccccc
?>
<html>
  <head>
    <title>APC Info</title>
    <meta http-equiv="Pragma" content="no-cache">
<style><!--
fieldset { border: #000000 solid 1px; background: <?php echo $bg?>; -moz-border-radius: 8px; border-radius: 8px;  }
legend { background: #9999cc; border: #000000 solid 1px; padding: 1px 10px }
table { border-spacing: 0; }
th { text-align: left; background: <?php echo $bg?>; border-bottom: #000000 dotted 1px; }
th.heading { border-bottom: #000000 solid 2px; text-align: right; }
th.lheading { border-bottom: #000000 solid 2px; text-align: left; }
th.left { text-align: right; right-padding: 10px; }
td { text-align: right; background: <?php echo $bg?>; padding-left: 10px; border-bottom: #000000 dotted 1px; }
td.section { text-align: center; background: #9999cc; border: #000000 solid 1px; padding-left: 0px; }
td.clear { border: 0px; font-size: small;}
td.name { text-align: left; border-bottom: #000000 dotted 1px; padding-left: 0px; }
td.value { text-align: right; border-bottom: #000000 dotted 1px; padding-left: 0px; }
td.heading { border-bottom: #000000 solid 2px; }
div.right { float: right; width: <?php echo $right_float_width ?>;}
div.left { margin: 0 0 0 0; <?php echo $left_float_style; ?> }
div.container { }
//-->
</style>

  </head>

  <body>
<?php

function box_start($title,$float='right') {
	echo <<<EOB
<div class="$float">
<fieldset>
<legend>$title</legend>
<table>

EOB;
}

function box_section_title($title) {
	echo "<tr><td colspan=\"2\" class=\"clear\">&nbsp;</td>\n";
	echo "</tr><tr><td colspan=\"2\" class=\"section\">$title</td></tr>\n";
	echo "<tr><td colspan=\"2\" class=\"clear\">&nbsp;</td>\n";
}

function box_rows($data,$headings="",$style="") {
	$th = $td = '';
	if(is_array($style)) foreach($style as $name=>$val) {
		$$name = " class=\"$val\"";
	} 
	if(is_array($headings)) {
		echo "<tr><th class=\"lheading\">".$headings[0]."</th><td class=\"heading\">".$headings[1]."</td></tr>\n";
	}
	foreach($data as $key=>$val) {
    	echo "<tr><th$th>$key</th><td$td>$val</td></tr>\n";
	}
}

function box_end() {
echo <<<EOB
</table>
</fieldset>
</div>
EOB;
}

function hit_sort( $a, $b ) {
	global $field;

    if ( $a['num_hits'] == $b['num_hits'] ) {
		return strcmp( strtolower( $a[$field] ), strtolower( $b[$field] ) );
	}

    return ($a['num_hits'] > $b['num_hits']) ? -1 : 1;
}

function field_sort( $a, $b ) {
	global $field;

	if( $a[$field] == $b[$field]) {
    	return ($a['num_hits'] > $b['num_hits']) ? -1 : 1;
	}
	return strcmp( strtolower( $a[$field] ), strtolower( $b[$field] ) );
}

function mtime_sort( $a, $b ) {
	global $field;

    if ( $a['mtime'] == $b['mtime'] ) {
		return strcmp( strtolower( $a[$field] ), strtolower( $b[$field] ) );
	}

    return ($a['mtime'] > $b['mtime']) ? -1 : 1;
}

function atime_sort( $a, $b ) {
	global $field;

    if ( $a['access_time'] == $b['access_time'] ) {
		return strcmp( strtolower( $a[$field] ), strtolower( $b[$field] ) );
	}

    return ($a['access_time'] > $b['access_time']) ? -1 : 1;
}

if(isset($_GET['sort'])) {
  switch($_GET['sort']) {
    case 'field':
	usort( $cache_info['cache_list'], 'field_sort' );
	break;
    case 'mtime':
	usort( $cache_info['cache_list'], 'mtime_sort' );
	break;
    case 'atime':
	usort( $cache_info['cache_list'], 'atime_sort' );
	break;
    default:
	usort( $cache_info['cache_list'], 'hit_sort' );
	break;
  }
} else usort( $cache_info['cache_list'], 'hit_sort' );

$delta = time() - $cache_info['start_time'];
$days = (int)($delta/86400);
$hours = (int)($delta/3600) - $days*24;
$mins = (int)($delta/60) - $hours*60 - $days*24*60;
$secs = $delta - $hours*3600 - $days*24*3600 - $mins*60;
if($days==1) $days = "1 day"; elseif($days) $days .= " days"; else $days='';

$data = array('PHP version'=>phpversion(),
              'uptime'=>sprintf("%s %d:%02d:%02d",$days,$hours,$mins,$secs),
              'num_seg' =>$sma_info['num_seg'],
              'seg_size'=>$sma_info['seg_size'],
              'used'=>($sma_info['seg_size']-$sma_info['avail_mem']),
              'avail_mem'=>$sma_info['avail_mem']);
box_start("APC Version ".phpversion('apc'));
box_rows($data);

$data = array('num_slots'=>$cache_info['num_slots'],
              'TTL'=>$ttl,
              'num_hits'=>$cache_info['num_hits'],
              'num_misses'=>$cache_info['num_misses'],
              'req/sec'=>sprintf("%.2f",($cache_info['num_hits']+$cache_info['num_misses'])/$delta),
              'cached_files'=>count($cache_info['cache_list']),
              'deleted_list'=>count( $cache_info['deleted_list']));
box_rows($data);
box_section_title("Advice");
if($sma_info['num_seg']>1) $advice = "This web interface doesn't support multiple segments. Set apc.shm_segments to 1";
else if(count($cache_info['cache_list']) > 3/4*$cache_info['num_slots'] && $cache_info['num_hits'] > 5000) {
  $advice = "You may want to increase apc.num_files_hint to ". (count($cache_info['cache_list'])*2) ." for better performance.";
}
else if(($sma_info['avail_mem']/$sma_info['seg_size'])<0.10) {
  $advice = "You are running low on shared memory.  It might be a good idea to increase apc.shm_size a bit.";
}
else if(((count($cache_info['cache_list']) / $cache_info['num_slots'])<0.10) && ($cache_info['num_hits'] > 5000)) {
  $advice = "You can lower your apc.num_files_hint to ".(count($cache_info['cache_list'])*2)." for better performance.";
} 
else $advice = "No problems detected";

echo "<tr><td colspan=\"2\" rowspan=\"2\" class=\"clear\" style=\"text-align: left;\">".wordwrap($advice,25,"<br />")."</td>\n";

box_end();
function menu() {
  global $mode;
?>
<form action="<?php echo getenv('SCRIPT_NAME')?>" method="GET">
<input type="submit" name="action" value="Top 25" />
<input type="submit" name="action" value="Top 100" />
<input type="submit" name="action" value="All" />
<?php if($mode=='user'):?>
<input type="submit" name="action" value="Opcode Cache" />
<input type="hidden" name="last_mode" value="user" />
<?php else:?>
<input type="submit" name="action" value="User Cache" />
<input type="hidden" name="last_mode" value="opcode" />
<?php endif;?>
<?php if(isset($_GET['sort'])) { ?>
<input type="hidden" name="sort" value="<?php echo htmlspecialchars((string)$_GET['sort'])?>" />
</form>
<?php } ?>
<?
}
?>

<div class="left">
<fieldset>
<legend>Cache Usage <?php echo sprintf("%.2f",100*(($sma_info['seg_size']-$sma_info['avail_mem'])/$sma_info['seg_size'])).'%'?></legend>
<?php
  $ptr = 0;
  $free = $sma_info['block_lists'][0];  // Only 1 segment supported for now
  foreach($free as $block) {
    if($block['offset']!=$ptr) {
      // Used block
      $size = sprintf("%.4f",(($block['offset']-$ptr)/($sma_info['seg_size']))*99);
echo <<<EOB
<div style="width: $size%; background: #000000; float:left; ">&nbsp;</div>
EOB;
    }
    $size = sprintf("%.4f",($block['size']/($sma_info['seg_size']))*99);
echo <<<EOB
<div style="width: $size%; background: #00ff00; float:left; ">&nbsp;</div>
EOB;
    $ptr = $block['offset']+$block['size']; 
  }
?>
</fieldset>
</div>
<br />
<?
if(isset($_GET['action']) && $_GET['action']!='Clear Cache') $action = '&action='.htmlspecialchars((string)$_GET['action']);
else $action='';
if($mode=='user')
     $name_sort = '<a href="'.getenv('SCRIPT_NAME') . '?sort=field'.$action.'">User Entry</a>';
else $name_sort = '<a href="'.getenv('SCRIPT_NAME') . '?sort=field'.$action.'">File Name (inode)</a>';
$mtime_sort = '<a href="'.getenv('SCRIPT_NAME') . '?sort=mtime'.$action.'">Last Modified</a>';
$atime_sort = '<a href="'.getenv('SCRIPT_NAME') . '?sort=atime'.$action.'">Last Accessed</a>';
$hit_sort = '<a href="'.getenv('SCRIPT_NAME') . '?sort=hit'.$action.'">Hits (ref count)</a>';
?>
<div class="left">
<fieldset>
<legend>Cached Files</legend>
<div style="text-align: right; margin: -10px 0 0 0; float: right;">
<?php menu() ?>
</div>
<br />
<table width="100%">
<tr>
<th class="lheading"><?php echo $name_sort?></th>
<th class="heading"><?php echo $mtime_sort?></th>
<th class="heading"><?php echo $atime_sort?></th>
<th class="heading"><?php echo $hit_sort?></th>
</tr>

<?php $cnt=0; foreach ( $cache_info['cache_list'] as $file ) { 
  $now = time();
  $atime = $file['access_time'];
  if($ttl && ($atime < ($now-$ttl))) $col = "#ffffcc";
  else $col = "#ccccff";
?>
<tr valign="baseline" bgcolor="#cccccc">
  <?php if($mode=='user'):?>
  <td class="name" bgcolor="<?php echo $col;?>"><?php echo $file[$field]; ?></td>
  <td class="value"><?php echo strftime("%x %X",$file['mtime'])?></td>
  <td class="value"><?php echo strftime("%x %X",$atime)?></td>
  <td class="value"><?php echo $file['num_hits'].' ('.$file['ref_count'].')'; ?></td>
  <?php else:?>
  <td class="name" bgcolor="<?php echo $col?>"><?php echo $file[$field].' ('.$file['inode'].')'; ?></td>
  <td class="value"><?php echo strftime("%x %X",$file['mtime'])?></td>
  <td class="value"><?php echo strftime("%x %X",$atime)?></td>
  <td class="value"><?php echo $file['num_hits'].' ('.$file['ref_count'].')'; ?></td>
  <?php endif;?>
</tr>
<?php $cnt++; if($limit && $cnt>$limit) break; } ?>
</table>
</fieldset>
</div>
<br />
</body>
</html>
