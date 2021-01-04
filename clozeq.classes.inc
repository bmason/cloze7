<?php

/**
 * @file
 * clozeq classes.
 *
 * This module uses the question interface to define the clozeq question type.
 *
 * A clozeq question requires one to fill in blanks by choosing.   

 */

/**
 * Extension of QuizQuestion.
 */
class clozeqQuestion extends QuizQuestion {

	private $phrasesCache = false;
	
  /**
   * Constructor
   *
   * @param $node
   *  clozeq node
   */
  public function __construct(stdClass $node) {
    parent::__construct($node);
    if (empty($this->node->match)) {
      $this->node->match = array();
    }
  }

  /**
   * Implementation of saveNodeProperties().
   *
   * @see QuizQuestion::saveNodeProperties()
   */
  public function saveNodeProperties($is_new = FALSE) {
    // Update or insert the question properties.
    db_merge('quiz_clozeq_properties')
      ->key(array(
        'nid' => $this->node->nid,
        'vid' => $this->node->vid,
      ))
      ->fields(array(
        'ignore_case' => intval($this->node->ignore_case),
      ))
      ->execute();


  }

  /**
   * Implementation of validateNode().
   *
   * @see QuizQuestion::validateNode()
   */
  public function validateNode(array &$form) {
    // No validation is performed.  Syntax errors will be obvious.

  }

   /**
   * Implementation of delete().
   *
   * @see QuizQuestion::delete()
   */
 public function delete($only_this_version = FALSE) {
    parent::delete($only_this_version);

    $delete_properties = db_delete('quiz_clozeq_properties')->condition('nid', $this->node->nid);

    if ($only_this_version) {
      $delete_properties->condition('vid', $this->node->vid);

      db_delete('quiz_clozeq_node')
        ->condition('nid', $this->node->nid)
        ->condition('vid', $this->node->vid)
        ->execute();
    }
    // Delete all versions of this question.
    else {

      db_delete('quiz_clozeq_node')
        ->condition('nid', $this->node->nid)
        ->execute();
    }

    $delete_properties->execute();
  }

  /**
   * Implementation of getNodeProperties().
   *
   * @see QuizQuestion::getNodeProperties()
   */
  public function getNodeProperties() {
    if (isset($this->nodeProperties)) {
      return $this->nodeProperties;
    }
    $props = parent::getNodeProperties();

    $res_a = db_query('SELECT ignore_case FROM {quiz_clozeq_properties}
            WHERE nid = :nid AND vid = :vid', array(':nid' => $this->node->nid, ':vid' => $this->node->vid))->fetchAssoc();
	

    if (is_array($res_a)) {
      $props = array_merge($props, $res_a);
    }


    $this->nodeProperties = $props;
    return $props;
  }

  /**
   * Implementation of getNodeView().
   *
   * @see QuizQuestion::getNodeView()
   */
  public function getNodeView() {
    $content = parent::getNodeView();  

   return $content;
  }
  
/*	$text is html
	for each 'blank' found [answer|feedback] in $text, answer a phrase:
	prefix - text before blank
	answer
	feedback
	suffix - trailing text - only the last blank */
  static function getPhrasesFrom($text) { 
	$text = str_ireplace('<p>', '<br>', $text);
	$text = str_ireplace('</p>', '', $text);


	//process tags from the media module [[<json>]]
	preg_match_all('/\[\[(.*?)\]\]/', $text ,$matches); 
	if (sizeof($matches[0])) foreach ($matches as $match) {
		$markup_for_media = media_wysiwyg_token_to_markup($match, TRUE);
		$text = str_replace($match, $markup_for_media, $text);
	}	
	

	$phrases = [];	
	while(strlen($text) > 0) {
		
		$phrase = [];
		$openBracket = strpos($text, '[');

		
		$phrase['prefix'] = substr($text, 0, $openBracket);
		$closeBracket = strpos($text, ']');
		$phrase['answer'] = substr($text, $openBracket+1, $closeBracket - $openBracket -1);
		if (($feedbackPos = strpos($phrase['answer'], '|')) !== false)	{
			$phrase['feedback'] = substr($phrase['answer'],$feedbackPos + 1);
			$phrase['answer'] = substr($phrase['answer'], 0, $feedbackPos);
		}
			
		if (strpos($text, '[', $closeBracket))
			$text = substr($text, $closeBracket+1);
		else {
			$phrase['suffix'] = substr($text, $closeBracket+1);
			$text = '';
		}
		$phrases[] = $phrase;
	}	  
	
	return $phrases;

  }
  
  public function getPhrases() {

	if(!$this->phrasesCache) 
		$this->phrasesCache = static::getPhrasesFrom(reset($this->node->body)[0]['value']); 
		
	return $this->phrasesCache;
	
  }

  /**
   * Implementation of getAnsweringForm().
   *
   * @see QuizQuestion::getAnsweringForm()
   */
  public function getAnsweringForm(array $form_state = NULL, $result_id) {
	  
	drupal_add_css ('.form-item.form-type-textfield {display: inline-block;}', 'inline');
    $form = parent::getAnsweringForm($form_state, $result_id); 
	
	
	foreach($this->getPhrases() as $phrase) 
		$maxSize = max($maxSize, strlen($phrase['answer']));
		
	$this->node->body = []; // instead of the body, display fields
	
    if (isset($result_id)) {
      // The question has already been answered. We load the answers.
      $response = new clozeqResponse($result_id, $this->node);
      $responses = $response->getResponse();
    }

	
	$id = 0;
	foreach($this->getPhrases() as $phrase) { 
		$form[$id] = array(
        '#type' => 'textfield',
		'#prefix'=>$phrase['prefix'],
		'#size' => $maxSize,
		'#default_value'=> isset($responses[$id]) ? $responses[$id]->answer : '', // restore previous answers 
		'#maxlength' => $maxSize,
		'#suffix' => $phrase['suffix'],
		'#weight'=>$id+30,
	  ); 
	  
	  $id++;
	}
	  
    return $form;
  }


  /**
   * Shuffles an array, but keep the keys.
   *
   * @param array $array
   *   Array to be shuffled
   * @return array
   *   A shuffled version of the array with keys preserved.
   */
  private function customShuffle(array $array = array()) {
    $new_array = array();
    while (count($array)) {
      $element = array_rand($array);
      $new_array[$element] = $array[$element];
      unset($array[$element]);
    }
    return $new_array;
  }


  public function getBodyFieldTitle() {
    return t('Instruction');
  }

  /**
   * Implementation of getCreationForm().
   *
   * @see QuizQuestion::getCreationForm()
   */
  public function getCreationForm(array &$form_state = NULL) {
    // Get the nodes settings, users settings or default settings.
    $default_settings = $this->getDefaultAltSettings();

    $form['settings'] = array(
      '#type' => 'fieldset',
      '#title' => t('Settings'),
      '#collapsible' => TRUE,
      '#collapsed' => FALSE,
    );
    $form['settings']['ignore_case'] = array(
      '#type' => 'checkbox',
      '#title' => t('Ignore case'),
      '#description' => t('Ignore the case of answers.'),
      '#default_value' => $default_settings['ignore_case'],
      '#parents' => array('ignore_case'),
    );


    return $form;
  }

  /**
   * Helper function providing the default settings for the creation form.
   *
   * @return array
   *  Array with the default settings
   */
  private function getDefaultAltSettings() {
    // If the node is being updated the default settings are those stored in the node.
    if (isset($this->node->nid)) {
      $settings['ignore_case'] = $this->node->ignore_case;
    }
    // The user is creating his first clozeq node.
    else {
      $settings['ignore_case'] = 0;
    }
    return $settings;
  }

  /**
   * Implementation of getMaximumScore().
   *
   * @see QuizQuestion::getMaximumScore()
   */
  public function getMaximumScore() {
    return sizeof($this->getPhrases());
  }

  /**
   * Get the correct answers for this question.
   *
   * @return array
   *  Array of correct answers
   */
  public function getCorrectAnswer() {
    $correct_answers = array();

	foreach($this->getPhrases() as $phrase)
		$correct_answers[] = $phrase['answer'];
    return $correct_answers;
  }

}

/**
 * Extension of QuizQuestionResponse
 */
class ClozeqResponse extends QuizQuestionResponse {

  /**
   * Constructor.
   *
   * @param int $result_id
   *   The result ID for the user's result set. There is one result ID per time
   *   the user takes a quiz.
   * @param stdClass $question_node
   *   The question node.
   * @param mixed $answer
   *   The answer (dependent on question type).
   */
  public function __construct($result_id, stdClass $question_node, $answer = NULL) {
    parent::__construct($result_id, $question_node, $answer);

    if (!isset($answer)) {
	$this->score = 0;
      $res = db_query('SELECT ua.answer, score, ua.match_id FROM {quiz_clozeq_user_answers} ua
              WHERE  ua.result_answer_id = :result_answer_id', array( ':result_answer_id' => $this->result_answer_id));
      $this->answer = array();
      while ($obj = $res->fetch()) {
        $this->answer[$obj->match_id] = $obj; 
		$this->score += $obj->score;
      } 
    } /* else
		if (!isset($this->score))
			$this->score(); 
    */

  }

  /**
   * Implementation of save().
   *
   * @see QuizQuestionResponse::save()
   */
  public function save() {
    $this->delete();
	$this->score = 0;
    $clozeqQuestion = new clozeqQuestion($this->question);
    $correct_answers = $clozeqQuestion->getCorrectAnswer();

    $insert = db_insert('quiz_clozeq_user_answers')->fields(array('match_id', 'result_answer_id', 'answer', 'score'));
    foreach ($this->answer as $key => $value) {
		if ($this->question->ignore_case)
			$this->score += ($score = (strtolower($correct_answers[$key]) == strtolower($value)) ? 1 : 0);
		else
			$this->score += ($score = ($correct_answers[$key] == $value) ? 1 : 0);
		$insert->values(array(
			'match_id' => $key,
			'result_answer_id' => $this->result_answer_id,
			'answer' =>  check_plain($value),  // remove all tags
			'score' => $score,
		));
    }
    $insert->execute();
  }

  /**
   * Implementation of delete().
   *
   * @see QuizQuestionResponse::delete()
   */
  public function delete() {
    db_delete('quiz_clozeq_user_answers')
      ->condition('result_answer_id', $this->result_answer_id)
      ->execute();
  }

  /**
   * Implementation of score().
   *
   * @see QuizQuestionResponse::score()
   */
  public function score() {  
	$this->question->answers[0]['score'] = $this->is_correct = $this->points_awarded =$this->score;  
	return $this->score;

  }

  /**
   * Implementation of getResponse().
   *
   * @see QuizQuestionResponse::getResponse()
   */
  public function getResponse() {
    return $this->answer;
  }

  /**
   * Implementation of getFeedbackValues().
   *
   * @see QuizQuestionResponse::getFeedbackValues()
   */
  public function getFeedbackValues() { 
    $data = array();
    $answers = $this->question->answers[0]['answer'];
	//incorporated into question display

    return $data;
  }

  /**
   * Get answers for a question in a result.
   *
   * This static method assists in building views for the mass export of
   * question answers.
   *
   * @see views_handler_field_prerender_list for the expected return value.
   */
  public static function viewsGetAnswers(array $result_answer_ids = array()) {
    $items = array();
    foreach ($result_answer_ids as $result_answer_id) {
      $ra = entity_load_single('quiz_result_answer', $result_answer_id);
      $question = node_load($ra->question_nid, $ra->question_vid);
      /** @var $ra_i QuizQuestionResponse */
      $ra_i = _quiz_question_response_get_instance($ra->result_id, $question);

      $matches = array();
      foreach ($question->match as $match) {
        $matches[$match['match_id']] = $match;
      }

      foreach ($ra_i->getResponse() as $question_id => $answer_id) {
        if (!empty($answer_id)) {
          $items[$ra->result_id][] = array('answer' => $matches[$question_id]['question'] . ': ' . $matches[$answer_id]['answer']);
        }
      }
    }

    return $items;
  }

}
