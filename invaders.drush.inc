<?php

/*
 * Space Invaders for Drush - andyw@circle, 18/02/2013
 * Requires php-ncurses
 */

/**
 * Implemention of hook_drush_command()
 */
function invaders_drush_command() {
	
	return array(
		'invaders' = array(
			'description' => dt("Space Invaders for Drush"),
			'arguments'   => array(),
			'options'     => array(),
		    'examples'    => array(),
		    'bootstrap'   => DRUSH_BOOTSTRAP_DRUSH // Don't bootstrap Drupal for this
		)
	);

}

/**
 * Implementation of hook_drush_help()
 */
function invaders_drush_help($section) {

	switch ($section) {
		case 'drush:invaders':
			return dt("Use left + right arrow keys, and space to fire.");
	}

}

/**
 * Main command hook
 */
function drush_invaders() {
	
	global $player, $enemies;

	// Outer loop, for multiple games
	while (true) {
	    
	    initializeGame();
	    
	    // Inner loop, for progressing through levels
	    while (true) {
	        
	        initializeLevel($player['level']);

	        // Main game loop for each level ..
	        for (;;) {
	            renderFrame();
	            if (empty($enemies) or $player['dead'])
	                break;
	            getKeyboardInput(&$keycode);
	            calculateNextFrame($keycode);
	            //checkForCollisions();
	            usleep(25000 - (($player['level'] - 1) * 3000)); // start at 25ms refresh, get faster
	        } 
	        
	        // We've exited the loop - player is either dead or has completed the level
	        if ($player['dead']) {
	            $player['lives']--;
	            $player['dead'] = false;
	        } else {
	            $player['level']++;
	        }
	        
	        if (!$player['lives'])
	            break;
	        
	    }
	    
	    if (!playAgain())
	        break;

	}
	stopItAndTidyUp();
}

/* Functions */
function calculateNextFrame($keycode) {
    
    global $enemies, $player, $screen, $missiles;
    
    static $direction = 'R';
    static $counter   = 0;

    // Perform enemy update on every 4th iteration (100ms)
    if (!(++$counter % 4)) {

	    // update enemy sprite position according to current direction of travel
	    foreach ($enemies as $key => $enemy) 
	    	$direction == 'R' ? $enemies[$key]['x']++ : $enemies[$key]['x']--;
	    
	    // if any sprites on screen x boundary, set $change_direction to true and break
	    foreach ($enemies as $enemy) 
	        if ($change_direction = ($direction == 'R' ? ($enemy['x'] >= ($screen['width'] - 3)) : !$enemy['x']))
	            break;

	    // If changing direction, switch direction flag and move enemy sprites down 1 in y
	    if ($change_direction) {
	        $direction = ($direction == 'R' ? 'L' : 'R');
	        foreach ($enemies as $key => $enemy)
	            $enemies[$key] = array(
	            	'type'     => $enemy['type'],
	            	'x'        => $enemy['x'], 
	            	'y'        => ++$enemy['y'],
	            	'can_fire' => $enemy['can_fire']
	            );  
	    }
	    // Check which enemies can fire, and fire 1 time out of 100 to begin with, reduce to 1 in 92 on level 2, etc ..
	    foreach ($enemies as $key => $enemy)
	    	if ($enemy['can_fire'] and !mt_rand(0, 100 - ($player['level'] * 8)))
				$missiles['enemy'][] = array(
					'x' => $enemy['x'] + 1,
					'y' => $enemy['y'] + 3
				);

    }

    // Perform missile update on every other iteration (50ms)
    if ($counter % 2) {
    	// Update position of player missiles
    	foreach ($missiles['player'] as $key => $missile)
    		// if missile y position > 1, move up one
    		if ($missile['y'] > 1)
	    		$missiles['player'][$key] = array(
	    			'x' => $missile['x'],
	    			'y' => --$missile['y']
	    		);
    		// remove missile when it reaches the top of the screen
    		else
    			unset($missiles['player'][$key]);
    	
    	// Update position of enemy missiles
    	foreach ($missiles['enemy'] as $key => $missile)
    		if ($missile['y'] < $screen['height'] - 1)
    			$missiles['enemy'][$key] = array(
    				'x' => $missile['x'],
    				'y' => ++$missile['y']
    			);
    		// remove missile when it reaches the bottom of the screen
    		else
    			unset($missiles['enemy'][$key]);
    }

    // Update player position on each iteration (25ms)
    if ($keycode == 'R' and $player['x'] <= ($screen['width'] - 5)) 
    	$player['x'] += 2;
    elseif ($keycode == 'L' and $player['x'])
    	$player['x'] -= 2;
    elseif ($keycode == ' ') 
    	$missiles['player'][] = array(
    		'x' => $player['x'] + 2,
    		'y' => $screen['height'] - 3
    	);
   	    
}

function checkForCollisions() {
    
    global $screen, $enemies;

    $max_y = $screen['height'] - 10; // or something (?)
    
    // If any enemy sprites have reached the bottom, kill player
    foreach ($enemies as $enemy) {
        if ($enemy['y'] > $max_y) {
            $dead = true;
            return;
        }
    }  
    
}

function getKeyboardInput(&$keycode) {
	
	global $input;

	// Attempt to read some keyboard input
	while (($buf = fgets($input, 4096)) !== false) 
    	$buffer .= $buf;

    // Buffer empty? Nothing to do, clear keycode
  	if (!$buffer) {
  		$keycode = '';
  		return;
  	}

  	switch (true) {
  		// Detect escape sequence for left arrow key
  		case substr($buffer, -2) == 'OD':
			$keycode = 'L';
			break;
		// Detect escape sequence for right arrow key
		case substr($buffer, -2) == 'OC':
			$keycode = 'R';
			break;
		// Space bar - easier
		case ' ':
			$keycode = ' ';
			break;
		case 'q':
			stopItAndTidyUp();

  	}

}

function initializeGame() {

    global $screen, $player, $window, $input;
    
    $player = array(
    	'score' => 0,
    	'level' => 1,
    	'lives' => 3
    );

    $screen = array(
     	'width'  => exec('tput cols'),
        'height' => exec('tput lines')
    );   

    // initialize ncurses, window and input stream
    ncurses_init();
    $window = ncurses_newwin(0, 0, 0, 0);
    
	$input = fopen('php://stdin','r');     // open direct input stream for reading
	stream_set_blocking($input, 0);        // set non-blocking mode
	system('tput civis');				   // hide cursor

}

function initializeLevel($level) {
    
    global $enemies, $player, $missiles;
    
    $player['dead'] = false;
    $player['x']    = 0;
    $enemies        = array();

    for ($y=0,$type=0;$y<20;$y+=4,$type+=0.5) 
        for ($x=0;$x<80;$x+=8) 
            $enemies[] = array(
                'type'     => floor($type),
                'x'        => $x,
                'y'        => $y,
                'can_fire' => ($y == 16 ? true : false) // bottom row only can fire to begin with
            );

    $missiles = array(
    	'enemy'  => array(),
    	'player' => array()
    );

    //unset($enemies[2][2]);
}

function playAgain() {
    return false;
}

function renderFrame() {

	global $screen, $player, $enemies, $missiles, $window;

	$score = sprintf("Score: %d", $player['score']);
	$lives = sprintf("Lives: %d", $player['lives']);
	$level = sprintf("Level: %d", $player['level']);

	ncurses_werase($window);

	ncurses_mvwaddstr($window, 0, 0, $score);
	ncurses_mvwaddstr($window, 0, ($screen['width'] / 2) - floor(strlen($level) / 2), $level);
	ncurses_mvwaddstr($window, 0, $screen['width'] - strlen($lives), $lives);
	
	// draw enemies
	foreach ($enemies as $enemy_key => $enemy) {
		// check for collisions with player missiles
		foreach ($missiles['player'] as $missile_key => $missile) {
			if ($missile['x'] >= $enemy['x'] and $missile['x'] <= ($enemy['x'] + 2) and $missile['y'] == ($enemy['y'] + 1)) {
				unset($missiles['player'][$missile_key]);
				unset($enemies[$enemy_key]);
				$player['score'] += 10;
				ncurses_mvwaddstr($window, $enemy['y'] + 2, $enemy['x'], "\\|/");
		        ncurses_mvwaddstr($window, $enemy['y'] + 3, $enemy['x'], "/|\\");
		        ncurses_beep();
		        // When enemy with firing capability dies, pass capability back to the enemy behind it ..
		        if ($enemy['can_fire']) 
			        foreach (array_reverse($enemies, true) as $key => $enemy2) 
			        	if ($enemy2['x'] == $enemy['x']) 
			        		$enemies[$key]['can_fire'] = true;
			        	   
				continue; 
			}
		}
		ncurses_mvwaddstr($window, $enemy['y'] + 2, $enemy['x'], "O.O");
		ncurses_mvwaddstr($window, $enemy['y'] + 3, $enemy['x'], ($enemy['x'] % 2 ? '/ \\' : '| |'));
	}

	// draw player missiles
	foreach ($missiles['player'] as $missile)
		ncurses_mvwaddstr($window, $missile['y'], $missile['x'], '*');

	// draw enemy missiles
	foreach ($missiles['enemy'] as $missile_key => $missile) {
		ncurses_mvwaddstr($window, $missile['y'], $missile['x'], '|');
		// check for collisions with player
		if ($missile['x'] >= $player['x'] and $missile['x'] < $player['x'] + 5 and $missile['y'] >= $screen['height'] - 3) {
			$player['dead'] = true;
			ncurses_beep();
			break;
		}
	}
	// draw player
	if ($player['dead']) {
		ncurses_mvwaddstr($window, $screen['height'] - 3, $player['x'], "\\ | /");
		ncurses_mvwaddstr($window, $screen['height'] - 2, $player['x'], "/ | \\");
	} else {
		ncurses_mvwaddstr($window, $screen['height'] - 3, $player['x'], ' ] [ ');
		ncurses_mvwaddstr($window, $screen['height'] - 2, $player['x'], '=====');
	}
	ncurses_wrefresh($window);

}

function stopItAndTidyUp() {
	ncurses_end();
	system('tput cnorm'); // show cursor
}
