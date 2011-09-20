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

  drupal_set_breadcrumb(array(
      l(t('Archives'), 'archives'),
      l(ppp_display_title_forum($forum, TRUE), "forum/{$forum->fid}", array('html' => TRUE)),
  ));

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
    'author' => array(
      'data' => t('Author'),
      'field' => 'author_pdn',
    ),
    'started' => array(
      'data' => t('Started'),
      // Sorting is redundant, but makes the interface more intuitive.
      'field' => 'tid',
    ),
    'ended' => array(
      'data' => t('Last post'),
      'field' => 'ended',
      'sort' => 'desc',
    ),
    'last' => t('Last poster'),
  );

  $page['topics'] = array(
    '#theme' => 'table',
    '#header' => $header,
    '#rows' => array(),
  );

  $topics = $forum->load_topics($header, $limit);
  foreach ($topics as $topic) {
    $query = db_select('ppp_post', 'p');
    $query->join('ppp_user', 'u', 'p.author = u.uid');
    $last = $query->fields('u', array('uid', 'pdn'))
      ->condition('topic', $topic->tid)
      ->orderBy('created', 'desc')
      ->range(0,1)
      ->execute()->fetch();
    $page['topics']['#rows'][$topic->tid] = array(
      'id' => $topic->tid,
      'title' => l($topic->title, "topic/{$topic->tid}"),
      'length' => $topic->length,
      'author' => l($topic->author_pdn, "user/{$topic->author}"),
      'started' => format_date($topic->started),
      'ended' => format_date($topic->ended),
      'last' => l($last->pdn, "user{$last->uid}"),
    );
  }

  return $page;
}

/**
 * Display one topic.
 */
function ppp_display_page_topic($topic) {
  $forum = $topic->get_forum();
  drupal_set_breadcrumb(array(
      l(t('Archives'), 'archives'),
      l(ppp_display_title_forum($forum, TRUE), "forum/{$forum->fid}", array('html' => TRUE)),
      l(ppp_display_title_topic($topic, TRUE), "topic/{$topic->tid}", array('html' => TRUE)),
  ));

  $pager = array('#theme' => 'pager');
  $posts = $topic->load_posts();
  $page = array(
    'pager_top' => $pager,
    'topic' => array(
      '#theme' => 'table',
      '#sticky' => FALSE,
      '#header' => array(t('Author'), t('Content')),
      '#rows' => array(),
      '#attributes' => array('class' => array('topic-table')),
    ),
    'pager_bottom' => $pager,
  );

  foreach ($posts as $pid => $post) {
    $page['topic']['#rows'][$pid] = array(
      'author' => array(
        'data' => theme('ppp_display_post_author', array(
          'topic' => $topic,
          'post' => $post,
          'user' => $post->get_author(),
        )),
        'valign' => 'top',
      ),
      'content' => theme('ppp_display_post_content', array(
        'topic' => $topic,
        'post' => $post,
        'user' => $post->get_author(),
      )),
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

function theme_ppp_display_post_author($variables) {
  $user = $variables['user'];
  $out = '<p><a class="username" href="' . url("user/{$user->uid}") . '">' . $user->pdn . '</a><br />';
  $out .= '<span class="title">' . $user->fields['title'] . '</span></p>';
  return $out;
}

function theme_ppp_display_post_content($variables) {
  $post = $variables['post'];
  $user = $variables['user'];

  // The following requires xbbcode, available at http://ermarian.net/xbbcode/
  if (filter_format_exists('xbbcode')) {
    $post->body = check_markup($post->body, 'xbbcode');
  }

  $out = '<p class="post-header" id="' . $post->pid . '">#' . $post->pid . '</p>';
  $out .= '<hr />';
  $out .= '<div class="post-body">' . $post->body . '</div>';
  $out .= '<hr />';
  $out .= '<p class="post-footer">';
  $out .= t('Posts: %posts', array('%posts' => $user->posts));
  if (isset($user->fields['location'])) {
    $out .= ' | ' . t('From %location', array('%location' => $user->fields['location']));
  }
  $out .= ' | ' . t('Registered %date', array('%date' => format_date($user->joined)));
  $out .= '</p>';
  return $out;
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