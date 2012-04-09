<?php
// $Id$


/**
 * @file spiderweb.parse.inc
 *   Contains all parser functions of the Spiderweb link.
 */

/**
 * Since the upgrade, Spiderweb's server switched from Pacific to Central time.
 */
define('SPIDERWEB_TIMEZONE', 'America/Chicago');

/**
 * Parse the front page of the board and retrieve category structure.
 *
 * @param $data
 *   string of HTML markup
 *
 * @return
 *   object { array $categories [ int $id => object $category { int $cid, string $name } ], array $fora [ int $id => object $forum ] }
 */
function spiderweb_parse_main_page($data) {
  module_load_include('inc', 'spiderweb', 'spiderweb.html');
  $main = new stdClass();
  // Yes. Fora.
  $main->fora = array();
  $main->categories = array();

  $html = spiderweb_html_parse($data);

  foreach ($html->find('thead') as $cat) {
    $link = $cat->find('a', 0);
    $id = trim(strrchr($link->href, '='), '=');
    $name = $link->innertext;
    $main->categories[$id] = (object)array('cid' => $id, 'name' => $name);
  }

  $cats = array_keys($main->categories);

  foreach ($html->find('tbody') as $i => $cat) {
    $rows = $cat->children;
    // table headers
    array_shift($rows);
    foreach ($rows as $row) {
      $link = $row->find('a', 0);
      if (!preg_match('/Board=([0-9]+)/', $link->href, $match)) {
        continue;
      }
      $id              = $match[1];
      $name            = $link->innertext;
      $description     = trim($row->find('div.forumdescript', 0)->innertext);
      $main->fora[$id] = (object)array(
        'fid' => $id,
        'name' => $name,
        'description' => $description,
        'category' => $cats[$i],
      );
    }
  }

  $html->clear();
  return $main;
}

/**
 * Parse one page of a forum topic list.
 *
 * @param $data
 *   A string of HTML markup.
 *
 * @return
 *   An array of topics [ int $post => object $topic { int $post, string $title, int $author, int $length, int $updated }].
 */
function spiderweb_parse_forum($data) {
  module_load_include('inc', 'spiderweb', 'spiderweb.html');
  $html = spiderweb_html_parse($data);

  $forum         = new stdClass();
  $forum->pages  = 1;
  $forum->topics = array();

  if (preg_match('%<td class="tdheader">Page [0-9]+ of ([0-9]+)</td>%', $data, $match)) {
    $forum->pages = $match[1];
  }

  $tables = $html->find('table.t_inner');
  do {
    $topics = array_shift($tables);
    /* UBB.threads bug: Sticky topics increase the number of topics on the first 
     * page, but are not subtracted from the total when calculating
     * pages = total / topics per page. The page count may be overestimated.
     * For example, on 361 topics with 1 sticky, page 19 will be available
     * but empty. Allow for this possibility.
     */
    if (!$topics) {
      $html->clear();
      return $forum;
    }
  }
  while (!preg_match('/<tr id="postrow-inline-[0-9]+">/', $topics->innertext));

  $topics = $topics->children;

  // headers.
  array_shift($topics);
  foreach ($topics as $topic) {
    $cells  = $topic->find('td');
    $links  = $cells[2]->find('a');
    $author = 0;

    foreach ($links as $link) {
      if (preg_match('/User=([0-9+])/', $link->href, $match)) {
        $author = $match[1];
        break;
      }
    }

    $date = $topic->find('span.date', 0)->innertext;
    if ($time = $topic->find('span.time', 0)) {
      $date = str_replace("at", "", "$date $time->innertext");
    }
    $t = (object)array(
      'post' => ltrim(strrchr($topic->id, '-'), '-'),
      'title' => html_entity_decode($links[0]->innertext),
      'author' => $author,
      'length' => trim($cells[3]->innertext) + 1,
      'updated' => spiderweb_parse_time($date),
    );
    $forum->topics[$t->post] = $t;
  }
  $html->clear();
  return $forum;
}

/**
 * Parse a topic on a single page view.
 *
 * @param $data
 *   The complete HTML content of the topic page. This must contain all posts of the topic.
 *
 * @return
 *   A topic object, including a $posts array.
 */
function spiderweb_parse_topic($data) {
  module_load_include('inc', 'spiderweb', 'spiderweb.html');
  $topic = (object)array();

  if (preg_match('/ubb=printthread.*Board=([0-9]+).*main=([0-9]+)/', $data, $match)) {
    $topic->forum = $match[1];
    $topic->tid = $match[2];
  }
  else {
    return FALSE;
  }

  $topic->closed = strpos($data, '<img src="/forum/images/general/default/lock.gif" alt="" />') > 0;

  if (preg_match('/<div id="prev-next-links">.*sticky=1/', $data)) {
    $topic->sticky = TRUE;
  }
  else $topic->sticky = FALSE;

  if (preg_match('%<title>(.*) - Spiderweb Software Forum</title>%', $data, $match)) {
    $topic->title = $match[1];
  }
  else {
    return FALSE;
  }

  // We have now parsed all the five "normal" topic properties. The other six are aggregated from post content, which is now parsed.

  // The following is a dirty way to split the posts, but it is both faster and
  // more reliable than regular expressions on large strings.
  $posts = strstr($data, '<td class="tdheader" style="cursor: pointer" id="rating_control"');
  $posts = substr($posts, 0, strpos($posts, '<div id="prev-next-links">'));
  $posts = explode('<a name="Post', $posts);
  array_shift($posts);

  $topic->posts = array();
  foreach ($posts as $post_data) {
    $post = spiderweb_parse_post($post_data);
    if ($post) {
      // Denormalize these IDs downwards:
      $post->topic = $topic->tid;
      $post->forum = $topic->forum;
      if (preg_match('%<div id="profile_popup_' . $post->pid . '".*?&User=(?P<author>[0-9]+)"%s', $data, $author)) {
        $post->author = $author['author'];
      }
      else {
        $post->author = 0;
      }
      $topic->posts[] = $post;
    } else {
      var_dump($post_data);
      throw Exception("Unparseable post.");
    }
  }

  $topic->post    = $topic->posts[0]->pid;
  $topic->length  = count($topic->posts);
  $topic->started = $topic->posts[0]->created;
  $topic->ended   = $topic->posts[$topic->length - 1]->created;
  $topic->author  = $topic->posts[0]->author;
  $topic->icon    = $topic->posts[0]->icon;

  return $topic;
}

/**
 * Parse one post.
 *
 * @param $data
 *   An object instance of the simpledom parser. (Strings WON'T WORK.)
 *
 * @return
 *   A post object { int $pid, int $created, int $icon, string $ip, string $body }
 *   Author is NOT included as that information is outside the post markup and must be added by the caller.
 */
function spiderweb_parse_post($data) {
  $post = (object) array();
  if (preg_match('%<span id="number(?P<pid>[0-9]+)">.*<span class="date">(?P<date>.*?)(?:at)?</span>(?: <span class="time">(?P<time>.*?)</span>)?%', $data, $meta)) {
    $post->pid = $meta['pid'];
    $date = $meta['date'];
    // Relative timestamps consist of two spans, and the "at" breaks strtotime.
    if (!empty($meta['time'])) {
      $date .= $meta['time'];
    }
    $post->created = spiderweb_parse_time($date);
  }
  else {
    return FALSE;
  }

  if (preg_match('%<div id="body[0-9]+">(?P<body>.*)</div>(?P<poll>.*)(?P<edit><span class="edited-wording">.*?</span>)\s+<(div|br)%s', $data, $body)) {
    $post->body = spiderweb_parse_bbcode_reverse($body['body'] . trim($body['poll']));
    if (preg_match('%<span class="small"><em>Edited by (?P<name>.*?) \(<span class="date">(?P<date>.*?)</span>\)(<br />Edit Reason: (?P<msg>.*?))?</em></span>%', $body['edit'], $edit)) {
      $post->edit_name = $edit['name'];
      $post->edit_date = spiderweb_parse_time($edit['date']);
      $post->edit_msg = isset($edit['msg']) ? $edit['msg'] : NULL;
    }
  }
  else {
    return FALSE;
  }

  if (preg_match('%<img src="/forum/images/icons/default/(?P<icon>.*?)" alt="" />%', $data, $icon)) {
    $post->icon = spiderweb_parse_icon_reverse($icon['icon']);
  }
  else {
    $post->icon = 0;
  }

  if ($ip = preg_match('%<i>\(?P<ip>((?:[0-9]{1,3}\.){3,3}\.[0-9])\)</i>%', $data, $ip)) {
    $post->ip = $ip['ip'];
  }
  else $post->ip = '';

  return $post;
}

/**
 * Reverses HTML markup into the original BBCode.
 *
 * @param object $dom
 *   An object, being the DOM element whose inner content should be reversed.
 *   Alternatively, a string.
 */
function spiderweb_parse_bbcode_reverse($html) {
  if (!$html) {
    return "";
  }
  module_load_include('inc', 'spiderweb', 'spiderweb.html');
  $dom = spiderweb_html_parse($html);
  if (!$dom) {
    throw new Exception("Post could not be parsed: " . check_plain($html));
  }
  $result = $dom->innertext;

  foreach ($dom->find('div.ubbcode-block') as $q) {
    $body = $q->find('div.ubbcode-body', 0);
    if (isset($body->children[0]) && $body->children[0]->tag == 'pre') {
      $tag = "[code]{$body->children[0]->innertext}[/code]";
    }
    elseif (isset($body->children[0]) && $body->children[0]->style == 'display: none;') {
      $tag = "[spoiler]{$body->children[0]->innertext}[/spoiler]";
    }
    elseif ($body) {
      $author = trim(strchr($q->find('div.ubbcode-header', 0)->innertext, ':'), ': ');
      $tag  = $author ? "[quote={$author}]{$body->innertext}[/quote]" : "[quote]{$body->innertext}[/quote]";
    }
    else {
      throw new Exception("Unrecognized block tag:" . check_plain($dom->innertext) . check_plain($result));
    }
    $result = str_replace($q->outertext, $tag, $result);
  }

  // Reparse now that the quotes are removed.
  $dom->clear();
  $dom = spiderweb_html_parse($result);
  $result = $dom->innertext;

  foreach ($dom->find('span[style]') as $span) {
    if (strpos($span->style, ': ') > 0) {
      list($property, $value) = explode(': ', $span->style, 2);
      $content = $span->innertext;
      if ($property == 'font-size') {
        $tag = "[size={$value}]{$content}[/size]";
      }
      elseif ($value == 'line-through') {
        $tag = "[s]{$content}[/s]";
      }
      elseif ($value == 'underline') {
        $tag = "[u]{$content}[/u]";
      }
      elseif ($value == 'bold') {
        $tag = "[b]{$content}[/b]";
      }
      elseif ($value == 'italic') {
        $tag = "[i]{$content}[/i]";
      }
      elseif ($property == 'color') {
        $tag = "[color={$value}]{$content}[/color]";
      }
      elseif ($property == 'font-family') {
        $tag = "[font={$value}]{$content}[/font]";
      }
      elseif ($property == 'vertical-align') {
        $tag = "[su{$value[2]}]{$content}[/su{$value[2]}]";
      }
      else {
        throw new Exception("Unrecognized span tag: $property, $value, $content");
      }
      if (isset($tag)) {
        $start = strpos($result, $span->outertext);
        $result = substr($result, 0, $start) . $tag . substr($result, $start + strlen($span->outertext));
      }
    }
  }

  /*foreach ($dom->find('a') as $a) {
    $url = "[url={$a->href}]{$a->innertext}[/url]";
    $result = str_replace($a->outertext, $url, $result);
  }*/
  // Rely on UBB not to emit broken tags.
  $result = preg_replace('%<a href="(.*?)".*?>%', '[url=$1]', $result);
  $result = str_replace('</a>', '[/url]', $result);
  $result = str_replace(array('<ul style="list-style-type: disc">', '</ul>', '<ol style="list-style-type: decimal">', '</ol>'), array('[list]', '[/list]', '[list=ol]', '[/list]'), $result);
  $result = preg_replace('%<li>(.*?)</li>%', '[*]$1' . "\n", $result);

  foreach ($dom->find('img') as $img) {
    if (dirname($img->src) == '/forum/images/graemlins/default') {
      $out = spiderweb_parse_smiley_reverse(basename($img->src));
    }
    else {
      $out = "[img]{$img->src}[/img]";
    }
    $result = str_replace($img->outertext, $out, $result);
  }

  $result = str_replace('<br />', "\n", $result);
  $dom->clear();

  return $result;
}

/**
 * Reverse lookup function for converting an image filename to an icon ID.
 *
 * @param $image
 *   A base filename with GIF extension.
 *
 * @return
 *   A numerical ID from 1 to 24, or 0 if the image is not found.
 */
function spiderweb_parse_icon_reverse($image) {
  static $icons = array(
    'thumbs_down.gif' => 1,
    'cool.gif' => 2,
    'sleep.gif' => 3,
    'sick.gif' => 4,
    'mad.gif' => 5,
    'lightbulb.gif' => 6,
    'eek.gif' => 7,
    'exclamation.gif' => 8,
    'confused.gif' => 9,
    'cry.gif' => 10,
    'grin.gif' => 11,
    'thumbs_up.gif' => 12,
    'book.gif' => 13,
    'tired.gif' => 14,
    'smirk.gif' => 15,
    'crazy.gif' => 16,
    'whistle.gif' => 17,
    'shocked.gif' => 18,
    'blush.gif' => 19,
    'wink.gif' => 20,
    'frown.gif' => 21,
    'laugh.gif' => 22,
    'smile.gif' => 23,
    'tongue.gif ' => 24,
  );

  return isset($icons[$image]) ? $icons[$image] : 0;
}

/**
 * Reverse lookup function for converting an emoticon image
 * to the proper markup.
 *
 * @param $image
 *   A GIF image filename with extension.
 *
 * @return
 *   Simple emoticon markup, or FALSE.
 */
function spiderweb_parse_smiley_reverse($image) {
  static $smileys = array(
    'rolleyesold.gif' => ':rolleyes:',
    'tongueold.gif' => ':P',
    'sadold.gif' => ':(',
    'eek.gif' => ':eek:',
    'mad.gif' => ':mad',
    'cool.gif' => ':cool:',
    'blush.gif' => ':blush:',
    'crazy.gif' => ':crazy',
    'laugh.gif' => ':lol:',
    'shocked.gif' => ':o',
    'smirk.gif' => ':/',
    'confused.gif' => ':confused:',
    'grin.gif' => ':D',
    'wink.gif' => ';)',
    'cry.gif' => ':cry:',
    'sick.gif' => ':sick:',
    'sleep.gif' => ':sleep:',
    'tired.gif' => ':tired:',
    'whistle.gif' => ':whistle:',
    'smileold.gif' => ':)',
    'smile.gif' => ':)',
  );
  return isset($smileys[$image]) ? $smileys[$image] : FALSE;
}

function spiderweb_parse_user($data) {
  $user = (object) array();
  $user->fields = array();

  // Take note here: Some of the spaces are actually U00A0 characters.
  if (preg_match('%<b>Member #:</b>.*?   ([0-9]+)  %s', $data, $match)) {
    $user->uid = $match[1];
  }
  else {
    if (strpos($data, 'This user is no longer in our database.')) {
      preg_match('/ubb%3Dshowprofile%26User%3D([0-9]+)/', $data, $match);
      $user->uid = $match[1];
      $user->del = TRUE;
      return $user;
    }
    return FALSE;
  }

  preg_match('%<title>Profile for (.*?) - Spiderweb Software Forum</title>%', $data, $match);
  $user->fields['displayed_name'] = $match[1];

  preg_match('%<span class="date">(.*?)(at)?</span>%', $data, $match);
  $date = $match[1];
  if (preg_match('%<span class="time">(.*?)</span>%', $data, $match)) {
    $date .= $match[1];
  }
  $user->joined = spiderweb_parse_time($date);

  preg_match('%<b>Total Posts:</b>.*?   ([0-9]+)  %s', $data, $match);
  $user->posts = (int) $match[1];

  preg_match('%<b>Title:</b>.*?   ([^\r\n]+)  %s', $data, $match);
  $user->fields['title'] = $match[1];

  if (preg_match('/<a href="mailto:(.+?)">\1<\/a>/', $data, $match)) {
    $user->fields['email'] = $match[1];
  }

  if (preg_match('/<a href="(.*?)" target="new">/', $data, $match)) {
    $user->fields['url'] = $match[1];
  }

  if (preg_match('%<td class="tdheader">.*?\'s Signature\s*</td>\s*</tr>\s*<tr>\s*<td class="alt-1">\s*(.+?)\s*</td>%s', $data, $match)) {
    $user->fields['signature'] = spiderweb_parse_bbcode_reverse($match[1]);
  }

  if (preg_match('%<tr><td>\s*Birthday:\s*</td><td>\s* (.+?)\s*</td></tr>%s', $data, $match)) {
    $user->fields['birthday'] = $match[1];
  }

  $fields = array(
    'im_icq' => 'IM \(ICQ\)',
    'im_yahoo' => 'IM \(YAHOO\)',
    'im_msn' => 'IM \(MSN\)',
    'im_aim' => 'IM \(AIM\)',
    'occupation' => 'Occupation',
    'hobbies' => 'Hobbies',
    'location' => 'Location',
    'real_name' => 'Real Name',
    'fav_game' => 'Favorite Spiderweb Game',
  );

  foreach ($fields as $name => $title) {
    if (preg_match('%<td valign=.top.>.*?' . $title . ':\s*?</td>\s*?<td valign=.top.>\s*? \s*?([^\s]([^\r\n]*?[^\s])?)\s*?</td>%s', $data, $match)) {
      $user->fields[$name] = $match[1];
    }
  }
  return $user;
}

function spiderweb_parse_time($text) {
  $tz = date_default_timezone_get();
  date_default_timezone_set(SPIDERWEB_TIMEZONE);
  $time = strtotime($text);
  date_default_timezone_set($tz);
  return $time;
}
