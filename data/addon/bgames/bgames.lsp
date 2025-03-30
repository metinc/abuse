
(perm-space)
(load "addon/pong/common.lsp")
(load "lisp/english.lsp")
(load "addon/pong/userfuns.lsp")
(load "gamma.lsp")
(setq chess_art (concatenate 'string chess_dir "chess.spe"))

(if (not (get_option "-net"))   ;;  are we connecting to a server?
  (start_server))
(load "lisp/chat.lsp")

(setq statbar_logo_x 260)
(setq statbar_logo_y 180)

(setq player1 'keyboard)
(setq player1-left  'left_arrow)
(setq player1-right 'right_arrow)

(setq statbar_health_x 272)
(setq statbar_health_y 1)
(setq statbar_health_bg_color 201)

(setf earth_x 3)
(setf earth_y 90)

(setq sfx_directory     "sfx/")
(load_big_font     "art/letters.spe" "letters")
(load_small_font   "art/letters.spe" "small_font")
(load_console_font "art/consfnt.spe" "fnt5x7")

(setq key_control       -1)
(load_color_filter "art/back/backgrnd.spe")
(load_palette      "art/back/backgrnd.spe")
(load_tiles chess_art "art/back/red.spe")

(setq joy_file          "art/joy.spe")
;(setq title_screen      (cons (concatenate 'string pong_dir "title.spe") "title_screen"))
(set_stat_bar chess_art "chess_statbar")

; ******** Music list ***************************
(setq song_list '("music/reggae.mus" "music/techno4.mus" "music/ballade.mus" "music/jazz.mus"))

(set_cursor_shape (def_image chess_art "mouse") 2 0)


(defun find_object_on_tile (x y)
  (let ((x (* (/ x 30) 30))              ;; snap to grid
	(y (* (/ y 20) 20)))
    (find_object_in_area (+ x 1) (+ y 1) (+ x 29) (+ y 19) peice_list)))

(defun player_move ()
  (let ((mex (if (< (player_pointer_x) 60)
		 60
	       (if (> (player_pointer_x) 359)
		   359
		 (player_pointer_x))))
	(mey (if (< (player_pointer_y) 30)
		 30
	       (if (> (player_pointer_y) 179)
		 179
		 (player_pointer_y)))))
	       
    (if (eq (total_objects) 1)  ;; we are moving a peice around
	(if (eq (player_b2_suggest) 0) ;; let go of button, put peice down
	    (progn	     
	      (with_object (get_object 0) 
			   (progn
			     (set_aistate 0)
			     (set_x mex) 
			     (set_y mey)))
	      (remove_object (get_object 0)))
	  (with_object (get_object 0) 
		       (progn 
			 (set_x mex) 
			 (set_y (+ mey 9)))))
      (let ((who (find_object_on_tile mex mey)))
	(if (eq (player_b2_suggest) 0)
	    (if (eq (player_b1_suggest) 0) nil
	      (if (and who (with_object who (and (eq (aistate) 3) 
						 (eq (otype) W_OTHELLO))))
		  (with_object who (set_aistate 2))))
	  (if (and who (with_object who (eq (aistate) 3)))
	      (progn
		(if (eq (with_object who (otype)) RESET)
		    (request_level_load start_file)
		  (link_object who))
		(with_object who (set_aistate 1))))))))
  0)
	  
	  
(defun player_draw ()
  (if (local_player)      
      (let ((x (* (/ (player_pointer_x) 30) 30));; snap to grid
	    (y (* (/ (player_pointer_y) 20) 20)))
	(let ((spot (game_to_mouse x y)))
	  (draw_rect (first spot)
		     (second spot)
		     (+ (first spot) 29)
		     (+ (second spot) 19)
		     (find_rgb 255 255 255)))))
  (if (edit_mode) (draw)))
  

(def_char PLAYER
  (funs (move_fun player_move)
        (draw_fun player_draw))
  (states chess_art (stopped "start")))

(def_char START
  (funs (ai_fun do_nothing)
        (draw_fun dev_draw))
  (states chess_art (stopped "start")))

(defun make_peice (sym name)
  (eval `(def_char ,sym (funs (ai_fun peice_ai)) (states ,chess_art (stopped ,name)))))

(make_peice 'W_PAWN "p1")
(make_peice 'B_PAWN "p2")
(make_peice 'W_KNIGHT "p3")
(make_peice 'B_KNIGHT "p4")

(make_peice 'W_BISHOP "p5")
(make_peice 'B_BISHOP "p6")

(make_peice 'W_CASTLE "p7")
(make_peice 'B_CASTLE "p8")

(make_peice 'W_QUEEN "p11")
(make_peice 'B_QUEEN "p12")

(make_peice 'W_KING "p9")
(make_peice 'B_KING "p10")

(make_peice 'R_CHECKER "r_check")
(make_peice 'B_CHECKER "b_check")

(make_peice 'R_CCHECK "r_check+")
(make_peice 'B_CCHECK "b_check+")


(defun othello_ai ()
  (if (eq (aistate) 2)
      (let ((last_state (state)))
	(if (next_picture) nil
	  (progn
	    (if (eq last_state stopped)
		(set_state running)
	      (set_state stopped))
	    (set_aistate 0))))
    (if (eq (aistate) 0)
	(progn
	  (set_aistate 3)
	  (set_x (+ (* (/ (x) 30) 30) 15))
	  (set_y (* (/ (+ (y) 19) 20) 20)))))
  T)
      
	    
(def_char W_OTHELLO 
  (funs (ai_fun othello_ai)) 
  (states chess_art 
	  (stopped (seq "otlo" 1 7))
	  (running (seq "otlo" 7 1))))

(def_char RESET 
  (funs (ai_fun do_nothing))
  (states chess_art (stopped "reset")))

(setq peice_list (list W_PAWN W_KNIGHT W_BISHOP W_CASTLE W_QUEEN W_KING
		       B_PAWN B_KNIGHT B_BISHOP B_CASTLE B_QUEEN B_KING
		       R_CHECKER B_CHECKER R_CCHECK B_CCHECK RESET W_OTHELLO))

(defun peice_ai ()
  (if (eq (aistate) 0)  ;; nobody is moving us around
      (progn
	(set_aistate 3)
	(set_x (+ (* (/ (x) 30) 30) 15))
	(set_y (* (/ (+ (y) 19) 20) 20))))

  T)

(create_players PLAYER)
(set_first_level start_file)
(gc)
(tmp-space)


