<?php
/**
 * @file
 * Edit form callback and related functions.
 */

/**
 * Edit form callback.
 *
 * @param $form
 * @param $form_state
 * @param $pid
 * @param $ds_id
 */
function islandora_content_model_forms_get_edit_form($form, &$form_state, $pid, $ds_id = NULL) {

  $page = $form_state['storage']['step'] = empty($form_state['storage']['step']) ? 1 : $form_state['storage']['step'];
  if ($page == 1) {
    $form_state['storage']['xml'] = TRUE;
    $form = islandora_content_model_forms_edit_page_1($pid, $ds_id, $form_state);
  }

  // If the form is set while fetching step 1, proceed to step 2 (case: one form).
  if (isset($form_state['values']['forms'])) {
    $page = 2;
    $form_state['storage']['step'] = 2;
  }

  if ($page == 2 && $form_state['storage']['xml']) {
    $form = islandora_content_model_forms_edit_page_2($pid, $form_state);
  }

  return $form;
}

/**
 * Datastream edit form step 1.
 *
 * @param $pid
 * @param $ds_id
 * @param $form_state
 * @throws Exception
 */
function islandora_content_model_forms_edit_page_1($pid, $ds_id, &$form_state) {

  $fedora_object = islandora_object_load($pid);
  $content_models = $fedora_object->models;
  if (empty($ds_id)) {
    $ds_id = NULL;
  }
  $form_names = array();
  $form_count = 0;
  foreach ($content_models as $model) {
    $forms = islandora_content_model_get_form_names($model, $ds_id);
    $cm_object = islandora_object_load($model);
    if($cm_object) {
      $cm_name = $cm_object->label;
    }
    else {
      $cm_name = $model;
    }
    $forms = array_keys($forms);
    foreach($forms as $form) {
      $form_names[$cm_name]["$model $form"] = $form;
      $form_count++;
    }
  }

  if ($form_count == 0) {
    drupal_set_message(t('No forms are associated. Please associate a form and try again.'), 'warning');
    drupal_goto("islandora/object/$pid");
  }

  $form = array();

  if ($form_count > 1) {
    $form['indicator'] = array(
      '#type' => 'fieldset',
      '#title' => t('Choose edit form.'),
    );
    $form['indicator']['forms'] = array(
      '#type' => 'select',
      '#title' => t('Metadata Forms Available'),
      '#options' => $form_names,
      '#description' => t('Select the form to edit.'),
    );
    $form['submit'] = array(
      '#type' => 'submit',
      '#validate' => array('islandora_content_model_forms_get_edit_form_validate'),
      '#value' => t('Next'),
    );
    $form['dsid'] = array(
      '#type' => 'hidden',
      '#value' => $ds_id,
    );
  }
  else {
    $model = key($form_names);
    $form_state['values']['forms'] = key($form_names[$model]);
    $form_state['values']['dsid'] = $ds_id;
  }

  return $form;
}

/**
 * Datastream edit form step 2.
 *
 * @param $pid
 * @param $form_state
 */
function islandora_content_model_forms_edit_page_2($pid, &$form_state) {

  list($content_model, $form_name) = explode(' ', $form_state['values']['forms'], 2);
  $dsid = $form_state['values']['dsid'];

  $fedora_object = islandora_object_load($pid);

  $xml = $fedora_object[$dsid];

  $form = xml_form_builder_get_form($form_state, $form_name, $xml->content);

  $form['content_model_pid'] = array(
    '#type' => 'hidden',
    '#value' => $content_model,
  );
  $form['forms'] = array(
    '#type' => 'hidden',
    '#value' => $form_state['values']['forms'],
  );
  $form['pid'] = array(
    '#type' => 'hidden',
    '#value' => $pid,
  );
  $form['dsid'] = array(
    '#type' => 'hidden',
    '#value' => isset($form_state['values']['dsid']) ? $form_state['values']['dsid'] : $dsid,
  );

  $form['#submit'] = array('islandora_content_model_forms_edit_form_submit');
  $form['#validate'] = array('islandora_content_model_forms_get_edit_form_validate');

  return $form;
}

/**
 * Edit form validation.
 *
 * @param $form
 * @param $form_state
 */
function islandora_content_model_forms_get_edit_form_validate($form, &$form_state) {
  if ($form_state['storage']['xml']) {
    if ($form_state['storage']['step'] == 1) {
      $form_state['storage']['step']++;
      $form_state['rebuild'] = TRUE;
    }
  }
}

/**
 * Edit form submit.
 *
 * @param $form
 * @param $form_state
 * @throws Exception
 */
function islandora_content_model_forms_edit_form_submit($form, &$form_state) {
  module_load_include('inc', 'xml_form_api', 'XMLForm');
  module_load_include('inc', 'islandora_content_model_forms', 'includes/ingest.form');
  global $base_url, $user;

  $pid = $form_state['values']['pid'];
  $dsid = $form_state['values']['dsid'];
  $form_name = $form_state['values']['forms'];
  $content_model_pid = $form_state['values']['content_model_pid'];
  $label_field = islandora_content_model_get_title_field($content_model_pid, $form_name);
  eval('$label = $form_state[\'values\']' . $label_field . ';');

  $xml_form = new XMLForm($form_state);
  $doc = $xml_form->submit($form, $form_state);
  $document = $doc->document;

  $object = islandora_object_load($pid);
  $object->label = $label;

  $xml_ds = $object[$dsid];
  $xml_ds->setContentFromString($document->saveXML());

  $transform = islandora_content_model_get_transform($content_model_pid, $form_name);
  if($transform) {
    $dc_doc = islandora_content_model_forms_transform_document($transform, $document, $object);
    $dc_ds = $object['DC'];
    $dc_ds->setContentFromString($dc_doc->saveXML());
  }

  $form_state['storage'] = NULL;
  $form_state['redirect'] = 'islandora/object/' . $pid;
}

