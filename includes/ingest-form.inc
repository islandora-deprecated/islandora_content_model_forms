<?php

function islandora_content_model_get_ingest_form($form, &$form_state, $collection_pid) {
  module_load_include('inc', 'islandora_content_model_forms', 'IngestObjectMetadataForm');
  $ingest_forms = new IngestObjectMetadataForm();
  
  $form = $ingest_forms->create($collection_pid, 'Test Label', $form_state);
  
  return $form;
}

function islandora_content_model_get_ingest_form_validate($form, &$form_state) {
//only validate the form if the submit button was pressed (other buttons may be used for AHAH
  if ($form_state['ahah_submission']) {
    $form_state['submitted'] = FALSE;
    return;
  }
  if (strpos($form_state['clicked_button']['#id'], 'edit-submit') === 0 && $form_state['ahah_submission'] != 1) {
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
// Get the uploaded file.
        $validators = array();

        if (!empty($_FILES['files']['name']['ingest-file-location'])) {
          $fileObject = file_save_upload('ingest-file-location', $validators);

          file_move($fileObject->filepath, 0, 'FILE_EXISTS_RENAME');
          $form_state['values']['ingest-file-location'] = $fileObject->filepath;
        }

        if (isset($form_state['values']['ingest-file-location']) && file_exists($form_state['values']['ingest-file-location'])) {
          /*
          module_load_include('inc', 'fedora_repository', 'ContentModel');
          module_load_include('inc', 'fedora_repository', 'MimeClass');

          $file = $form_state['values']['ingest-file-location'];

          $contentModelPid = ContentModel::getPidFromIdentifier($form_state['values']['models']);
          $contentModelDsid = ContentModel::getDSIDFromIdentifier($form_state['values']['models']);

          if (($cm = ContentModel::loadFromModel($contentModelPid, $contentModelDsid)) !== FALSE) {
            $allowedMimeTypes = $cm->getMimetypes();

            $mimetype = new MimeClass();
            $dformat = $mimetype->getType($file);

            if (!empty($file)) {
              if (!in_array($dformat, $allowedMimeTypes)) {
                form_set_error('ingest-file-location', 
                    t('The uploaded file\'s mimetype') . 
                    ' (' . $dformat . ') ' .
                    t('is not associated with this Content Model. The allowed types are') . 
                    ' ' . implode(' ', $allowedMimeTypes));
                file_delete($file);
                return;
              }
              elseif (!$cm->execIngestRules($file, $dformat)) {
                drupal_set_message(t('Error following Content Model Rules'), 'error');
                foreach (ContentModel::$errors as $err) {
                  drupal_set_message(check_plain($err), 'error');
                }
              }
            }
          }
        }
        */
        $form_state['rebuild'] = FALSE;
        break;
    }
  }
}

function islandora_content_model_ingest_form_submit($form, &$form_state) {
//only validate the form if the submit button was pressed (other buttons may be used for AHAH
  if ($form_state['ahah_submission']) {
    $form_state['submitted'] = FALSE;
    return;
  }
  //ddebug_backtrace();
  if ($form_state['storage']['xml'] && module_exists('islandora_content_model_forms')) {
    module_load_include('inc', 'islandora_content_model_forms', 'IngestObjectMetadataForm');
    $xml_form = new IngestObjectMetadataForm();
    $xml_form->submit($form, $form_state);
  }
  elseif (strpos($form_state['clicked_button']['#id'], 'edit-submit') === 0) {
    global $base_url;
    module_load_include('inc', 'fedora_repository', 'CollectionClass');
    module_load_include('inc', 'fedora_repository', 'CollectionPolicy');
    module_load_include('inc', 'fedora_repository', 'ContentModel');

    $contentModelPid = ContentModel::getPidFromIdentifier($form_state['values']['models']);
    $contentModelDsid = ContentModel::getDSIDFromIdentifier($form_state['values']['models']);
    $err = TRUE;
    $redirect = TRUE;
    if (($cp = CollectionPolicy::loadFromCollection($form_state['values']['collection_pid'])) !== FALSE) {
      $relationship = $cp->getRelationship();

      if (($cm = ContentModel::loadFromModel($contentModelPid, $contentModelDsid)) !== FALSE) {
        $pid = $cp->getNextPid($contentModelDsid);
        global $user;
        $form_state['values']['user_id'] = $user->name;
        $form_state['values']['pid'] = $pid;
        $form_state['values']['content_model_pid'] = $contentModelPid;
        $form_state['values']['relationship'] = $relationship;

        $err = (!$cm->execFormHandler($form_state['values'], $form_state));

        $_SESSION['fedora_ingest_files'] = ''; //empty this variable

        $attr = $cm->getIngestFormAttributes();
        $redirect = $attr['redirect'];

        if ($redirect) {
          $form_state['storage'] = NULL;
        }
      }
    }

    if ($redirect) {
      $form_state['redirect'] = ($err) ? ' ' : url("fedora/repository/{$form_state['values']['collection_pid']}");
    }
  }
}
}  
  /*
  module_load_include('inc', 'islandora_content_model_forms', 'IngestObjectMetadataForm');
  $ingest_form = new IngestObjectMetadataForm();
  $ingest_form->submit($form, $form_state);
  */

function islandora_content_model_forms_page_1($collection_pid, $collection_label, array &$form_state) {
    //ddebug_backtrace();
    $potential_models = $this->getPotentialContentModels($collection_pid, $form_state);
    unset($potential_models['islandora:collectionCModel/ISLANDORACM']);
    list($identifier, $name) = array_peek($potential_models);
    $selected_model = isset($form_state['values']['models']) ? $form_state['values']['models'] : $identifier;
    $identifier = isset($form_state['post']['models']) ? $form_state['post']['models'] : $identifier;
    //$content_model_pid = ContentModel::getPidFromIdentifier($identifier);
    $content_model_pid = 'islandora:sp_strict_pdf';
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
          $collection_link = l("Islandora Collection Manager", "https://wiki.duraspace.org/display/ISLANDORA6121/Collection+Manager");
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
      '#description' => t('Content models define datastream composition, relationships between this and other content models, and the mandatory behaviors associated with each digital object.<br /> Additional information may be found <a href="https://wiki.duraspace.org/display/FEDORACREATE/Content+Models+Overview">here.</a> '),
      
      '#ajax' => array(
        'callback' => 'islandora_content_model_update_form_choice',
        'wrapper' => 'content-form-choice',
        'effect' => 'fade',
      ),
      
    /*
      '#ahah' => array(
        'path' => 'islandora/form/update',
        'wrapper' => 'content-form-choice',
        'effect' => 'fade',
      ),
      */
    );

    $form['indicator']['content_form_ahah_wrapper'] = array(
      '#prefix' => '<div id="content-form-choice">',
      '#suffix' => '</div>',
      '#type' => 'item',
    );

    if (count($form_names) == 1) {
      $form['indicator']['content_form_ahah_wrapper']['forms'] = array(
        '#type' => 'hidden',
        '#value' => array_shift(array_keys($form_names)),
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
      '#submit' => array('islandora_content_model_ingest_form_submit'),
      '#value' => t('Next'),
    );
    
    return $form;
}