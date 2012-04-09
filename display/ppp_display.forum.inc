<?php

/**
 * @file
 * Page callback for the forum page.
 */

/**
 * Display one forum.
 */
function _ppp_display_page_forum($forum) {
  if (!$forum) {
    return MENU_NOT_FOUND;
  }

  // TODO: User config.
  $limit = 25;

  drupal_set_breadcrumb(array(
    l(t('Archives'), 'archives'),
    l($forum->name, 'forum/' . $forum->fid),
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
      'field' => 'started',
    ),
    'ended' => array(
      'data' => t('Last post'),
      'field' => 'ended',
      'sort' => 'desc',
    ),
    'last' => array(
      'data' => t('Last poster'),
      'field' => 'last_pdn',
    ),
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
    $page['topics']['#rows'][$topic->tid] = array(
      'id' => $topic->tid,
      'title' => l($topic->title, "topic/{$topic->tid}"),
      'length' => $topic->length,
      'author' => l($topic->author_pdn, "user/{$topic->author}"),
      'started' => format_date($topic->started),
      'ended' => format_date($topic->ended),
      'last' => l($topic->last_pdn, "user{$topic->last_uid}"),
    );
  }

  return $page;
}

