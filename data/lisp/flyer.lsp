;; Copyright 1995 Crack dot Com,  All Rights reserved
;; See licensing information for more details on usage rights


(defun flyer_explosive_damage (from)
  (and from
       (with_object from
	 (or (eq (otype) GRENADE)
	     (eq (otype) FIREBOMB)
	     (eq (otype) ROCKET)
	     (eq (otype) DFRIS_BULLET)
	     (eq (otype) STRAIT_ROCKET)
	     (eq (otype) BOMB)
	     (eq (otype) BIG_BOMB)
	     (eq (otype) CONC)
	     (eq (otype) CONC_AIR)))))

(defun flyer_explode ()
  (play_sound GRENADE_SND 127 (x) (y))
  (add_object EXPLODE1 (+ (x) (random 10)) (+ (+ (random 10) (y)) -20)     0)
  (add_object EXPLODE1 (- (x) (random 10)) (+ (- (y) (random 10)) -20)     2)
  (add_object EXPLODE1 (x) (+ (- (y) (random 20)) -20)                     4)
  (with_object (bg) (set_kills (+ (kills) 1)))
  nil)

(defun flyer_fall_ai ()
  ;; Keep spinning and smoking until the regular character physics reports
  ;; the first collision.
  (if (eq (mod (state_time) 2) 0)
      (add_object SMALL_DARK_CLOUD (x) (y)))
  (if (not (next_picture))
      (set_state turn_around))
  (if (eq (move 0 0 0) 0)
      T
    (flyer_explode)))

(defun flyer_ai ()
  (if (not (eq smoke_time 0))                 ;; if we just got hit, put out some smoke
      (progn
	(setq smoke_time (- smoke_time 1))
	(if (eq (mod smoke_time 2) 0)
	    (add_object SMALL_DARK_CLOUD (x) (y)))))

  (if (eq (aistate) 0)                        ;; wait for something to turn us on
      (if (or (eq (total_objects) 0) (not (eq (with_object (get_object 0) (aistate)) 0)))
	  (if (next_picture) T
	    (progn
	      (set_targetable T)
	      (set_state running)
	      (set_aistate 1)))
	(progn
	  (set_targetable nil)
	  (set_state stopped)
	  T))
    (if (eq (hp) 0)
	(if (eq (aistate) 2)                   ;; non-explosive kill: fall first
	    (flyer_fall_ai)
	  (flyer_explode))                     ;; explosive kill: explode now
      (progn
	(if (eq (mod (state_time) 5) 0)      ;; make flyer noise every 5 ticks
	    (play_sound FLYER_SND 127 (x) (y)))
	(if (> (with_object (bg) (x)) (x))   ;; start going right if player is to the right
	    (progn
	      (set_xvel (+ (xvel) 1))
	      (if (> (xvel) max_xvel) (set_xvel max_xvel))
	      (if (eq (direction) -1)
		  (progn
		    (set_direction 1)
		    (set_state turn_around))))
	  (if (< (with_object (bg) (x)) (x))  ;; start going left if the player is to the left
	      (progn
		(set_xvel (- (xvel) 1))
		(if (< (xvel) (- 0 max_xvel)) (set_xvel (- 0 max_xvel)))
		(if (eq (direction) 1)
		    (progn
		      (set_direction -1)
		      (set_state turn_around))))))
	(if (> (with_object (bg) (- (y) 70)) (y))
	    (if (> (yvel) max_yvel)
		(set_yvel (- (yvel) 1))
	      (set_yvel (+ (yvel) 1)))

	  (if (< (with_object (bg) (- (y) 50)) (y))
	      (if (< (yvel) (- 0 max_yvel))
		  (set_yvel (+ (yvel) 1))
		(set_yvel (- (yvel) 1)))))

	(if (eq (random 5) 0)                ;; add some randomness to the movement
	    (set_xvel (+ (xvel) 1))
	  (if (eq (random 5) 0)
	      (set_xvel (- (xvel) 1))))
	(if (eq (random 5) 0)
	    (set_yvel (+ (yvel) 1))
	  (if (eq (random 5) 0)
	      (set_yvel (- (yvel) 1))))

	(if (next_picture) T (set_state running))  ;; reset animation when done

	(bounce_move '(set_xvel (/ (xvel) 2)) '(set_xvel (/ (xvel) 2))
		     '(set_yvel (/ (yvel) 2)) '(set_yvel (/ (yvel) 2)) nil)

	(if (> fire_time 0)              ;; if we need to wait till next burst
	    (progn
	      (setq fire_time (- fire_time 1))
	      (if (eq fire_time 0)
		  (progn
		    (setq burst_left burst_total)
		    (setq burst_wait 0))))
	  (if (eq burst_wait 0)
	      (if (and (< (distx) 150) (eq (direction) (facing)))
		  (let ((firex (+ (x) (* (direction) 10)) )
			(firey (y))
			(playerx (+ (with_object (bg) (x)) (with_object (bg) (* (xvel) 4))))
			(playery (+ (- (with_object (bg) (y)) 15) (with_object (bg) (* (yvel) 2)))))
		    (if (and (can_see (x) (y) firex firey nil) (can_see firex firey playerx playery nil))
			(progn
			  (let ((angle (atan2 (- firey playery)
					      (- playerx firex))))
			    (if (or (eq burst_left 1) (eq burst_left 0))
				(setq fire_time fire_delay)
			      (setq burst_left (- burst_left 1)))
			    (setq burst_wait burst_delay)
			    (fire_object (me) (aitype) firex firey angle (bg))
			    )))))
	    (setq burst_wait (- burst_wait 1))))
	T))))



(defun flyer_cons ()
  (setq fire_delay 20)
  (setq burst_delay 3)
  (setq max_xvel 10)
  (setq max_yvel 5)
  (set_aitype 9)
  (setq burst_total 2))


(defun flyer_damage (amount from hitx hity push_xvel push_yvel)
  (if (and from (with_object from (and (> (total_objects) 0)
				       (with_object (get_object 0)
						    (or (eq (otype) FLYER)
							(eq (otype) GREEN_FLYER))
							))))
      nil
    (if (eq (state) stopped) nil
      (progn
	(damage_fun amount from hitx hity push_xvel push_yvel)
	(if (<= (hp) 0)
	    (if (flyer_explosive_damage from)
		(progn
		  (setq smoke_time 0)
		  (set_aistate 3))
	      (progn
		(setq smoke_time 0)
		(set_targetable nil)
		(set_aistate 2)
		(set_state turn_around)
		(set_gravity 1)
		(set_xacel 0)
		(set_fxacel 0)
		(set_yacel 0)
		(set_fyacel 0)
		(set_yvel 1)
		(set_fyvel 0)))
	  (progn
	    (setq smoke_time 30)
	    (set_yvel (- (yvel) 14))
	    (set_state flinch_up)))))))



(def_char FLYER
  (funs (ai_fun flyer_ai)
	(damage_fun  flyer_damage)
	(constructor flyer_cons))

  (flags (hurtable T))
  (abilities (start_hp 20))
  (vars fire_delay burst_delay burst_total burst_wait burst_left
	max_xvel   max_yvel    smoke_time fire_time)
  (fields ("fire_delay"  who_fdelay)
	  ("burst_delay" who_bdelay)
	  ("burst_total" who_btotal)
	  ("max_xvel"    who_mxv)
	  ("max_yvel"    who_myv)
	  ("hp"          ai_health)
	  ("aitype"      ai_type)
	  ("aistate"     ai_state))

  (range 200 200)
  (states "art/flyer.spe"
	  (running (seq "ffly" 1 12))
	  (stopped (seq "unhurtable" 1 7))
	  (flinch_up  '("flinch" "flinch" "flinch"))
	  (turn_around (seq "ftrn" 1 6))))




(def_char GREEN_FLYER
  (funs (ai_fun flyer_ai)
	(damage_fun  flyer_damage)
	(constructor flyer_cons))

  (flags (hurtable T))
  (abilities (start_hp 20))
  (vars fire_delay burst_delay burst_total burst_wait burst_left
	max_xvel   max_yvel    smoke_time fire_time)
  (fields ("fire_delay"   who_fdelay)
	  ("burst_delay"  who_bdelay)
	  ("burst_total"  who_btotal)
	  ("max_xvel"     who_mxv)
	  ("max_yvel"     who_myv)
	  ("hp"           ai_health)
	  ("aitype"       ai_type)
	  ("aistate"      ai_state))

  (range 200 200)
  (states "art/flyer.spe"
	  (running (seq "gspe" 1 7))
	  (stopped (seq "gdrp" 1 12))
	  (flinch_up  '("ghurt" "ghurt" "ghurt"))
	  (turn_around (seq "gspn" 1 7))))
