<?php
// $Id$


/**
 * @file ppp_display.pages.inc
 * The pages that show the PPP content.
 */

/**
 * Display the archive main page
 */
function ppp_display_page_main() {
  module_load_include('inc', 'ppp_data', 'ppp_data.load');
  $categories = ppp_data_load_categories();
  $page = array();
  $header = array(
    'id' => t('#'),
    'forum' => t('Forum'),
    'topics' => t('Topics'),
    'posts' => t('Posts'),
    'last' => t('Last activity'),
  );
  foreach ($categories as $cid => $name) {
    $page[$cid] = array(
      '#theme' => 'table',
      '#caption' => $name,
      '#header' => $header,
      '#rows' => array(),
    );

    $fora = ppp_data_load_fora($cid);

    foreach ($fora as $fid => $forum) {
      // Run the fora through the access layer
      if (!ppp_display_access_forum($forum)) {
        unset($fora[$fid]);
        continue;
      }
      $last = ppp_data_load_lastpost($fid);

      $page[$cid]['#rows'][$fid] = array(
        'id' => $fid,
        'name' => theme('ppp_display_forum_name', array('forum' => $forum)),
        'topics' => $forum->topics,
        'posts' => $forum->posts,
        'last' => $last ? theme('ppp_display_forum_last', array('post' => $last)) : t('<span style="white-space:nowrap">No content</span>'),
      );
    }
  }
  return $page;
}

/**
 * Display one forum
 */
function ppp_display_page_forum($forum) {
  // TODO: User config.
  $limit = 25;

  $breadcrumb = drupal_set_breadcrumb(array(l(t('Archives'), 'archives'), l(ppp_display_title_forum($forum, TRUE), "forum/{$forum->fid}")));

  $page = array(
    'description' => array(
      '#markup' => $forum->description,
    ),
    'pager' => array(
      '#theme' => 'pager',
    ),
  );

  $header = array(
    'id' => array(
      'data' => t('#'),
      'field' => 'tid',
    ),
    'title' => array(
      'data' => t('Title'),
      'field' => 'title',
    ),
    'length' => array(
      'data' => t('Posts'),
      'field' => 'length',
    ),
    'author' => t('Author'),
    'started' => t('Started'),
    'ended' => array(
      'data' => t('Last post'),
      'field' => 'ended',
      'sort' => 'desc',
    ),
  );

  $page['topics'] = array(
    '#theme' => 'table',
    '#header' => $header,
    '#rows' => array(),
  );

  $topics = $forum->load_topics($header, $limit);
  foreach ($topics as $topic) {
    $page['topics']['#rows'][$topic->tid] = array(
      'id' => $topic->tid,
      'title' => l($topic->title, "topic/{$topic->tid}"),
      'length' => $topic->length,
      'author' => _ppp_display_userlink($topic->author, $topic->started),
      'started' => format_date($topic->started),
      'last' => format_date($topic->ended),
    );
  }

  return $page;
}

/**
 * Render the name and description of a forum for the main page.
 */
function theme_ppp_display_forum_name($variables) {
  $forum = $variables['forum'];
  $link['attributes'] = array(
    'class' => 'forum-link',
    'id' => "forum-link-{$forum->fid}",
  );
  return l($forum->name, "forum/{$forum->fid}", $link) . '<p class="forum-desc">' . check_plain($forum->description) . '</p>';
}

/**
 * Render the last post field.
 */
function theme_ppp_display_forum_last($variables) {
  $post = $variables['post'];
  return t('<span style="white-space: nowrap">In <a href="@url">%topic</a></span> <span style="white-space: nowrap">by !user</span> <span style="white-space: nowrap"> at %time</p>', array('@url' => url('topic/' . $post->tid), '%topic' => $post->title, '!user' => _ppp_display_userlink($post->author, $post->ended), '%time' => format_date($post->ended)));
}

/**
 * Create a link to a user using a time-appropriate name if possible.
 */
function _ppp_display_userlink($uid, $date = NULL) {
  module_load_include('inc', 'ppp_data', 'ppp_data.load');
  $user = ppp_data_load_user($uid, $date);
  if ($user) {
    return l($user->fields['displayed_name'], "user/$uid");
  }
  else {
    return t('User #%uid', array('%uid' => $uid));
  }
}