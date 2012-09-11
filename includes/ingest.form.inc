<?php
/**
 * @file
 * Ingest form callback and related functions.
 */

/**
 * Multi-step ingest form callback.
 * Step 1: determine desired XML form.
 * Step 2: render XML form.
 *
 * @param $form
 * @param $form_state
 * @param $collection_pid
 * @throws Exception
 */
function islandora_content_model_forms_get_ingest_form($form, &$form_state, $collection_pid) {
  module_load_include('module', 'islandora', 'islandora');
  $page = $form_state['storage']['step'] = empty($form_state['storage']['step']) ? 1 : $form_state['storage']['step'];
  $collection_object = islandora_object_load($collection_pid);
  $collection_label = $collection_object->label;

  if ($page == 1) {
    $form_state['storage']['xml'] = TRUE;
    $form_state['storage']['collection_pid'] = $collection_pid;
    $form = islandora_content_model_forms_ingest_page_1($form_state, $collection_pid, $collection_object->label);
  }
  elseif ($form_state['storage']['xml']) {
    $form_state['storage']['content_model'] = $form_state['values']['models'];
    $form = islandora_content_model_forms_ingest_page_2($form_state, $collection_pid, $collection_object->label);
  }
  else {
    throw new Exception("Couldn't Create the Ingest Form.");
  }

  return $form;
}

/**
 * Ingest form step 1.
 *
 * @param $form_state
 * @param $collection_pid
 * @param $collection_label
 */
function islandora_content_model_forms_ingest_page_1(&$form_state, $collection_pid, $collection_label) {
  $object = islandora_object_load($collection_pid);
  drupal_set_title(t('Ingest'));
  $potential_models = islandora_content_model_forms_get_potential_content_models($object, $form_state);
  unset($potential_models['islandora:collectionCModel/ISLANDORACM']);
  unset($potential_models['islandora:collectionCModel']);

  list($identifier, $name) = array_peek($potential_models);
  $selected_model = isset($form_state['values']['models']) ? $form_state['values']['models'] : $identifier;
  $content_model_pid = $selected_model;

  $form_names = islandora_content_model_get_form_names($content_model_pid);

  $form = array();
  if (count($potential_models) == 0) {
    if (!user_access('manage collections')) {
      $message = t('This collection cannot receive additions.  Please contact the site administrator if you believe this message to be in error.');
    }
    else {
      if (module_exists('islandora_collection_manager')) {
        $message = t('Select the %manage tab to add content models or child collections to this collection', array('%manage' => 'Manage This Collection')); //XXX: Should get the value from the collection manager?
      }
      else {
        $collection_link = l(t("Islandora Collection Manager"), "https://wiki.duraspace.org/display/ISLANDORA6121/Collection+Manager");
        $message = t('This collection requires either one or more content models in its COLLECTION_POLICY, or one or more child collections.<br />
                      We strongly recommend enabling the !collection_link module for managing collection objects.', array('!collection_link' => $collection_link));
      }
      $form['message'] = array(
        '#type' => 'item',
        '#value' => $message,
      );
      return $form;
    }
  }

  $form['indicator'] = array(
    '#type' => 'fieldset',
    '#title' => t('Ingest digital object into %collection_label (@collection_pid), Step #1:', array('@collection_pid' => $collection_pid, '%collection_label' => $collection_label)),
  );

  $form['indicator']['models'] = array(
    '#type' => 'select',
    '#title' => t('Content models available'),
    '#options' => $potential_models,
    '#default_value' => $selected_model,
    '#description' => t('Content models define datastream composition, relationships between this and other content models, and the mandatory behaviors associated with each digital object.<br /> Additional information may be found <a href="https://wiki.duraspace.org/display/FEDORACREATE/Content+Models+Overview">here.</a>'),
    '#ajax' => array(
      'callback' => 'islandora_content_model_forms_update_form_choice',
      'wrapper' => 'content-form-choice',
      'effect' => 'fade',
      'method' => 'replace',
    ),
  );

  $form['indicator']['content_form_ahah_wrapper'] = array(
    '#prefix' => '<div id="content-form-choice">',
    '#suffix' => '</div>',
    '#type' => 'item',
  );

  if (count($form_names) == 1) {
    $keys = array_keys($form_names);
    $form['indicator']['content_form_ahah_wrapper']['forms'] = array(
      '#type' => 'hidden',
      '#value' => array_shift($keys),
    );
  }
  elseif (count($form_names) > 0) {
    $form['indicator']['content_form_ahah_wrapper']['forms'] = array(
      '#type' => 'select',
      '#title' => t('Select form'),
      '#options' => $form_names,
      '#description' => t('Select the form to populate the metadata of the new object.'),
    );
  }

  $form['collection_pid'] = array(
    '#type' => 'hidden',
    '#value' => $collection_pid,
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#validate' => array('islandora_content_model_forms_get_ingest_form_validate'),
    '#value' => t('Next'),
  );

  return $form;
}

/**
 * Ingest form step 2.
 *
 * @param $form_state
 * @param $collection_pid
 * @param $collection_label
 */
function islandora_content_model_forms_ingest_page_2(&$form_state, $collection_pid, $collection_label) {
  drupal_set_title(t('Ingest form'));
  $content_model_pid = $form_state['values']['models'];
  $content_model_dsid = $content_model_pid; //@TODO: check what this should be

  $form_name = NULL;
  if (isset($form_state['values']['forms'])) {
    $form_name = $form_state['values']['forms'];
  }
  else {
    $form_names = islandora_content_model_get_form_names($content_model_pid);
    reset($form_names);
    $form_name = key($form_names);
    $form_state['values']['forms'] = $form_name;
  }

  if (empty($form_name)) {
    //@TODO: what should happen here
  }
  $dsid = islandora_content_model_get_dsid($content_model_pid, $form_state['values']['forms']);

  $xml = NULL;
  if (!empty($form_state['storage']['xml_data'])) {
    $xml = $form_state['storage']['xml_data'];
  }

  $form = xml_form_builder_get_form($form_state, $form_name, $xml);
  $form['collection_pid'] = array(
    '#type' => 'hidden',
    '#value' => $collection_pid,
  );
  $form['content_model_pid'] = array(
    '#type' => 'hidden',
    '#value' => $content_model_pid,
  );
  $form['content_model_dsid'] = array(
    '#type' => 'hidden',
    '#value' => $content_model_dsid,
  );
  $form['dsid'] = array(
    '#type' => 'hidden',
    '#value' => $dsid,
  );
  $form['models'] = array(
    '#type' => 'hidden',
    '#value' => $form_state['values']['models'],
  );
  $form['forms'] = array(
    '#type' => 'hidden',
    '#value' => $form_state['values']['forms'],
  );

  $form['#submit'] = array('islandora_content_model_forms_ingest_form_submit');
  $form['#validate'] = array('islandora_content_model_forms_get_ingest_form_validate');
  return $form;
}

/**
 * Ingest form validation.
 *
 * @param $form
 * @param $form_state
 */
function islandora_content_model_forms_get_ingest_form_validate($form, &$form_state) {
  // Only validate the form if the submit button was pressed (other buttons may be used for AJAX)
  if (!$form_state['submitted']) {
    return;
  }

  // Check for submission was here. Seems to be not needed now?
  switch ($form_state['storage']['step']) {
    case 1:
      $form_state['storage']['step']++;
      $form_state['rebuild'] = TRUE;
      break;

    case 2:
      // XML based form.
      if ($form_state['storage']['xml']) {
        module_load_include('inc', 'xml_form_api', 'XMLForm');
        $xml_form = new XMLForm($form_state);
        $xml_form->validate($form, $form_state);
      }

      // Handle uploaded files.
      $arr = array();
      /*
      if (!empty($types_allowed)) {
        foreach ($types_allowed as $type) {
          $arr[] = $mimetype->getExtension($type);
        }
      }
      */

      if (!empty($_FILES['files']['name']['ingest-file-location'])) {
        $file_object = file_save_upload('ingest-file-location', array('file_validate_extensions' => $arr));

        if ($file_object) {
          $form_state['values']['ingest-file-location'] = $file_object->uri;
          $form_state['values']['ingest-file-location-id'] = $file_object->fid;
        }
      }
      $form_state['rebuild'] = FALSE;
      break;
  }
}

/**
 * Ingest form submit.
 *
 * @param $form
 * @param $form_state
 */
function islandora_content_model_forms_ingest_form_submit($form, &$form_state) {
  // Only submit the form if the submit button was pressed (other buttons may be used for AJAX)
  if (!$form_state['submitted']) {
    return;
  }

  module_load_include('inc', 'xml_form_api', 'XMLForm');
  module_load_include('inc', 'islandora', 'includes/islandora.ingest');
  global $base_url, $user;

  if (isset($_FILES['files']['name']['file_upload']) && $_FILES['files']['type']['file_upload'] == 'text/xml') {
    $tmp_dir = file_directory_path();
    if (!$file = file_save_upload('file_upload', array('file_validate_extensions' => array()), $tmp_dir)) {
      drupal_set_message(t('No XML file loaded'), 'warning');
      return;
    }
    $file = "$tmp_dir/{$file->filename}";
    $xml_file = file_get_contents($file);
    $document = DOMDocument::loadXml($xml_file);
    unlink($file);
    if (!$document) {
      drupal_set_message(t('This does not appear to be a valid XML file'), 'warning');
      return;
    }
  }
  else {
    $xml_form = new XMLForm($form_state);
    $doc = $xml_form->submit($form, $form_state);
    $document = $doc->document;
  }

  $collection_pid = $form_state['values']['collection_pid'];
  $content_model_pid = $form_state['values']['content_model_pid'];
  $content_model_dsid = $form_state['values']['content_model_dsid'];
  $dsid = $form_state['values']['dsid'];
  $form_name = $form_state['values']['forms'];
  $should_redirect = TRUE;

  $relationship = NULL;
  $pid = NULL;

  $fedora_object = islandora_object_load($collection_pid);
  $collection_policy_info = islandora_ingest_get_information($fedora_object);

  $relationship = $collection_policy_info['relationship'];

  $content_models = array(array('pid' => $content_model_pid));

  $object = islandora_ingest_get_object($content_models, $collection_pid, $collection_policy_info['relationship'], 'islandora');
  $transform = islandora_content_model_get_transform($content_model_pid, $form_name);
  $label_field = islandora_content_model_get_title_field($content_model_pid, $form_name);
  eval('$label = $form_state[\'values\']' . $label_field . ';');

  $object->label = drupal_substr($label, 0, 256);
  $object->owner = $user->name;

  if (!isset($form['#post']['active-toggle'])) {
    $state = 'A';
  }
  else {
    $state = $form['#post']['active-toggle'] ? 'A' : 'I';
  }
  $object->state = $state;

  $ds = $object->constructDatastream($dsid, 'X');
  $ds->setContentFromString($document->saveXML());
  $ds->label = $dsid . ' Record';
  $object->ingestDatastream($ds);

  if ($dsid != 'DC') {
    $dc_ds = $object->constructDatastream('DC', 'X');
    $dc_ds->label = 'DC Record';
    $object->ingestDatastream($dc_ds);
    $dc_doc = islandora_content_model_forms_transform_document($transform, $document, $object);
    $dc_ds->setContentFromString($dc_doc->saveXML());
  }

  if (!empty($relationship) && !empty($object->id)) {

    // added ne functionality to static thumbnails to objects if values are provided in forms
    $module = isset($form_state['values']['module']) ? $form_state['values']['module'] : NULL;
    $image_dir = isset($form_state['values']['image_directory']) ? $form_state['values']['image_directory'] : NULL;
    $thumbnail_file = isset($form_state['values']['thumbnail_file']) ? $form_state['values']['thumbnail_file'] : NULL;

    if ($module && $thumbnail_file) {
      $stored_thumbnail = drupal_get_path('module', $module) . "/$image_dir/$thumbnail_file";
      $stored_thumbnail = preg_replace('/\/\//', '/', $stored_thumbnail);
      file_copy($stored_thumbnail, file_directory_path());
      $thumbnail = file_directory_path() . '/' . $thumbnail_file;
      $_SESSION['fedora_ingest_files']['TN'] = $thumbnail;
    }

    $uploaded_file = isset($form_state['values']['ingest-file-location']) && trim($form_state['values']['ingest-file-location']) !== '';
    $ingest_file_location = $uploaded_file ? $form_state['values']['ingest-file-location'] : NULL;
    if (isset($ingest_file_location)) {
      $ingest_file_location = is_array($ingest_file_location) ? $ingest_file_location : array('OBJ' => $ingest_file_location);
    }

    islandora_content_model_forms_ingest_files($ingest_file_location, $object);

    $form_state['storage'] = NULL;
  }

  $form_state['redirect'] = "islandora/object/{$collection_pid}";
  $object = islandora_ingest_add_object($object);

  if (isset($form_state['values']['ingest-file-location-id'])) {
    file_delete(file_load($form_state['values']['ingest-file-location-id']));
  }
}

/**
 * AJAX callback for step 1 form selection.
 *
 * @param $form
 * @param $form_state
 */
function islandora_content_model_forms_update_form_choice($form, $form_state) {
  return $form['indicator']['content_form_ahah_wrapper'];
}

/**
 * Get content models associated with a collection.
 *
 * @param $collection_pid
 * @param $form_state
 */
function islandora_content_model_forms_get_potential_content_models($collection_object, $form_state = array()) {
  module_load_include('inc', 'islandora', 'includes/islandora.ingest');
  $info = islandora_ingest_get_information($collection_object);

  $potential_models = array();
  if (isset($info['models']) && is_array($info['models'])) {
    foreach ($info['models'] AS $model) {
      $potential_models[$model['pid']] = $model['name'];
    }
  }

  if (isset($form_state['storage']['content_model']) && $content_models = $form_state['storage']['content_model']) {
    $content_models = (array) $content_models;
    foreach ($content_models as $content_model_pid) {
      try {
        $content_model = $collection_object->repository->getObject($content_model_pid);
        $potential_models["$content_model_pid"] = $content_model->label;
      }
      catch (RepositoryException $e) {

      }
    }
  }

  return $potential_models;
}

/**
 * Transform form generated XML.
 *
 * @param $transform
 * @param $source_document
 * @param $object
 */
function islandora_content_model_forms_transform_document($transform, $source_document, $object) {
  if ($transform !== NULL) {
    $xsl = new DOMDocument();
    $xsl->load($transform);
    $xslt = new XSLTProcessor();
    $xslt->importStyleSheet($xsl);
    $document = $xslt->transformToDoc($source_document);
    if ($document) {
      $xpath = new DOMXPath($document);
      // Set the Label
      $results = $xpath->query("*[local-name()='title']");
      $results->item(0)->nodeValue = $object->label;
      // Set the Pid
      $results = $xpath->query("*[local-name()='identifier']");
      $results->item(0)->nodeValue = $object->id;
      if (isset($document->documentElement)) {
        return $document;
      }
    }
  }
  return NULL;
}

/**
 * Add a managed datastream to an object based on an uploaded file.
 *
 * @param $ingest_file_locations
 * @param $object
 */
function islandora_content_model_forms_ingest_files($ingest_file_locations, $object) {
  if (empty($ingest_file_locations) || count($ingest_file_locations) === 0) {
    return;
  }
  foreach ($ingest_file_locations as $dsid => $file) {
    list($label, $mime_type) = _islandora_content_model_forms_get_file_attributes($file);

    $ds = $object->constructDatastream($dsid, 'M');
    $file = drupal_realpath($file);
    $ds->setContentFromFile($file);
    $ds->label = $label;
    $ds->mimetype = $mime_type;
    $object->ingestDatastream($ds);
  }
}

/**
 * Determine a title and mimetype for a file.
 *
 * @param $file
 */
function _islandora_content_model_forms_get_file_attributes($file) {
  module_load_include('inc', 'islandora', 'includes/mime.detect');
  global $base_url;

  $mime = new MimeDetect();
  $mime_type = $mime->getMimetype($file);
  $parts = explode('/', $file);
  foreach ($parts as $n => $part) {
    $parts[$n] = rawurlencode($part);
  }
  $path = implode('/', $parts);
  $file_url = $base_url . '/' . $path;
  $begin_index = strrpos($file_url, '/');
  $dtitle = drupal_substr($file_url, $begin_index + 1);
  $dtitle = urldecode($dtitle);
  return array($dtitle, $mime_type);
}
