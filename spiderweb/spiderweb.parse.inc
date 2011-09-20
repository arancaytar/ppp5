<?php
// $Id$


/**
 * @file spiderweb.parse.inc
 *   Contains all parser functions of the Spiderweb link.
 */

define('TIME_OFFSET', 9 * 3600);

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

  if (preg_match('/Page [0-9]+ of ([0-9+])/', $html->find('td.tdheader', 0)->innertext, $match)) {
    $forum->pages = $match[1];
  }

  $topics = $html->find('table.t_inner', 1)->children;
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
    // obsolete when the user uses non-relative times.
    if ($time = $topic->find('span.time', 0)) {
      $date = str_replace("at", "", "$date $time->innertext");
    }
    $t = (object)array(
      'post' => ltrim(strrchr($topic->id, '-'), '-'),
      'title' => html_entity_decode($links[0]->innertext),
      'author' => $author,
      'length' => trim($cells[3]->innertext) + 1,
      'updated' => strtotime($date) - TIME_OFFSET,
    );
    if (!strtotime($date)) {
      var_dump($date, check_plain($topic->innertext));
      return $forum;
    }
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

  $topic->posts = array();
  preg_match_all('$<table width="100%" cellspacing="0" cellpadding="0">(?<post>.*?)</table>$s', $data, $posts);
  foreach ($posts['post'] as $post_data) {
    $post = spiderweb_parse_post($post_data);
    if ($post) {
      // Denormalize these IDs downwards:
      $post->topic = $topic->tid;
      $post->forum = $topic->forum;
      preg_match('%<div id="profile_popup_' . $post->pid . '".*?&amp;User=(?P<author>[0-9]+)"%s', $data, $author);
      $post->author = $author['author'];

      $topic->posts[] = $post;
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
    $post->created = strtotime($date);
  }
  else {
    return FALSE;
  }

  if (preg_match('%<div id="body[0-9]+">(?P<body>.*)</div>%s', $data, $body)) {
    $post->body = spiderweb_parse_bbcode_reverse($body['body']);
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
function spiderweb_parse_bbcode_reverse($dom) {
  module_load_include('inc', 'spiderweb', 'spiderweb.html');
  if (!is_object($dom)) {
    $dom = spiderweb_html_parse($dom);
    $clear = TRUE;
  }
  else {
    $clear = FALSE;
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
    else {
      $author = trim(strchr($q->find('div.ubbcode-header', 0)->innertext, ':'), ': ');
      $tag  = $author ? "[quote={$author}]{$body->innertext}[/quote]" : "[quote]{$body->innertext}[/quote]";
    }
    $result = str_replace($q->outertext, $tag, $result);
  }

  // Reparse now that the quotes are removed.
  if ($clear) $dom->clear();
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
      else {
        $span->dump();
        var_dump($property, $value, $content);
      }
      $result = str_replace($span->outertext, $tag, $result);
    }
  }

  foreach ($dom->find('a') as $a) {
    $url = "[url={$a->href}]{$a->innertext}[/url]";
    $result = str_replace($a->outertext, $url, $result);
  }

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
  $result = html_entity_decode($result);
  if ($clear) {
    $dom->clear();
  }
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

  if (preg_match('%<b>Member #:</b>.*?&nbsp;  ([0-9]+) &nbsp;%s', $data, $match)) {
    $user->uid = $match[1];
  }
  else {
    return FALSE;
  }

  preg_match('%<title>Profile for (.*?) - Spiderweb Software Forum</title>%', $data, $match);
  $user->fields['displayed_name'] = $match[1];

  preg_match('%<span class="date">(.*?)(at)?</span>%', $data, $match);
  $date = $match[1];
  if (preg_match('%<span class="time">(.*?)</span>%', $data, $match)) {
    $date .= $match[1];
  }
  $user->joined = strtotime($date) - TIME_OFFSET;

  preg_match('%<b>Total Posts:</b>.*?&nbsp;  ([0-9]+) &nbsp;%s', $data, $match);
  $user->posts = (int) $match[1];

  preg_match('%<b>Title:</b>.*?&nbsp;  ([^\r\n]+) &nbsp;%s', $data, $match);
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
    if (preg_match('%<td valign=.top.>.*?' . $title . ':\s*?</td>\s*?<td valign=.top.>\s*?&nbsp;\s*?([^\s]([^\r\n]*?[^\s])?)\s*?</td>%s', $data, $match)) {
      $user->fields[$name] = check_plain($match[1]);
    }
  }
  return $user;
}