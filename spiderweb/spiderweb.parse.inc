<?php
// $Id: spiderweb.parse.inc 478 2010-08-16 11:58:31Z arancaytar $


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
  module_load_include('inc', 'spiderweb', 'spiderweb.parse');
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
  module_load_include('inc', 'spiderweb', 'spiderweb.parse');
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
  $html = spiderweb_html_parse($data);

  $topic = (object)array();

  foreach ($html->find('#options_popup a') as $link) {
    if (preg_match('/ubb=printthread.*Board=([0-9]+).*main=([0-9]+)/', $link->href, $match)) {
      $topic->forum = $match[1];
      $topic->tid = $match[2];
    }
  }

  if ($html->find('td.subjecttable img[source=/forum/images/general/default/lock.gif]', 0)) {
    $topic->closed = TRUE;
  }
  else $topic->closed = FALSE;

  if (preg_match('/sticky=1/', $html->find('div#prev-next-links a', 0)->href)) {
    $topic->sticky = TRUE;
  }
  else $topic->sticky = FALSE;

  // Yes, we calculate the string length of a constant here. You will thank me when you next need to maintain this, Aran.
  $topic->title = substr($html->find('title', 0)->innertext, 0, -1 * strlen(' - Spiderweb Software Forum'));

  // We have now parsed all the five "normal" topic properties. The other six are aggregated from post content, which is now parsed.

  $topic->posts = array();

  foreach ($html->find('table.t_outer > tr > td > table.t_inner > tr > td > table') as $post_data) {
    $post = spiderweb_parse_post($post_data);
    if ($post) {
      // Denormalize these IDs downwards:
      $post->topic = $topic->tid;
      $post->forum = $topic->forum;

      preg_match('/User=([0-9]+)/', $html->find("#profile_popup_{$post->pid} a", 0)->href, $match);
      $post->author = $match[1];

      $topic->posts[] = $post;
    }
  }

  $topic->post    = $topic->posts[0]->pid;
  $topic->length  = count($topic->posts);
  $topic->started = $topic->posts[0]->created;
  $topic->ended   = $topic->posts[$topic->length - 1]->created;
  $topic->author  = $topic->posts[0]->author;
  $topic->icon    = $topic->posts[0]->icon;

  $html->clear();
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
  $meta = $data->find('td.subjecttable span span');
  if (count($meta) < 2) return FALSE;

  $post          = new stdclass();
  $post->pid     = trim($meta[0]->innertext, '#');

  // Relative timestamps consist of two spans, and the "at" breaks strtotime.
  $date = $meta[1]->innertext;
  if (count($meta) > 2) {
    $date = str_replace('at', '', $date) . $meta[2]->innertext;
  }
  $post->created = strtotime($date) - TIME_OFFSET;

  $img = $data->find('td.subjecttable a img', 0);
  $post->icon = isset($img->src) ? spiderweb_parse_icon_reverse(basename($img->src)) : 0;

  if ($ip = $data->find('td.author-content span.small i', 0)) {
    preg_match('/\(([0-9\.]+)\)/', $ip->innertext, $match);
    $post->ip = $match[1];
  }
  else $post->ip = '';

  $post->body = spiderweb_parse_bbcode_reverse($data->find('div.post_inner div', 0));

  // Obviously, since topic and author are denormalized downward, they must be added by the caller.
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
      $code = "[code]{$body->children[0]->innertext}[/code]";
      $result = str_replace($q->outertext, $code, $result);
    }
    else {
      $author = trim(strchr($q->find('div.ubbcode-header', 0)->innertext, ':'), ': ');
      $quote  = $author ? "[quote={$author}]{$body->innertext}[/quote]" : "[quote]{$body->innertext}[/quote]";
      $result = str_replace($q->outertext, $quote, $result);
    }
  }

  // Reparse now that the quotes are removed.
  $dom = spiderweb_html_parse($result);

  foreach ($dom->find('span') as $span) {
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
    $result = str_replace($span->outertext, $tag, $result);
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

