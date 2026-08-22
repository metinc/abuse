(defun zap (zapdam zapspd x y angle)
	(with_object (add_object LASR_BULLET x y)
		      (progn
			(setq sgb_lifetime 10)
			(setq sgb_speed zapspd)
			(setq sgb_dam zapdam)
			(setq sgb_lastx (x))
			(setq sgb_lasty (y))
			(setq sgb_angle angle)
			(setq sgb_bright_color (find_rgb 255 245 235))
			(setq sgb_medium_color (find_rgb 150 145 140))
			(if creator
			    (progn
			      (setq sgb_speed (+ sgb_speed (/ (xvel) 2)))
			      (link_object creator)))
			(sgun_ai)
			))
)

;; Custom weapon types 11-13, dispatched by the shared fire_object function.
(defun leon_fire_object (creator type x y angle target)
  (select type
    (11
      (play_sound SHIP_ZIP_SND 127 (x) (y))
      (zap 8 18 x y angle))
    (12
      (play_sound SHIP_ZIP_SND 127 (x) (y))
      (zap 8 22 x y angle)
      (zap 8 22 x y (+ angle 14))
      (zap 8 22 x y (- angle 14)))
    (13
      (play_sound SHIP_ZIP_SND 127 (x) (y))
      (zap 6 24 x y angle)
      (zap 6 20 x y (+ angle 15))
      (zap 6 20 x y (- angle 15))
      (zap 6 16 x y (+ angle 30))
      (zap 6 16 x y (- angle 30)))))

(defun las_ai ()
  (setq sgb_lastx (x))
  (setq sgb_lasty (y))
  (set_course sgb_angle sgb_speed)
  (if (eq sgb_lifetime 0)
      nil
    (let ((bx (bmove (if (> (total_objects) 0) (get_object 0) nil))))  ; don't hit the guy who fired us.
      (setq sgb_lifetime (- sgb_lifetime 1))
      (if (eq bx T) T
	(progn
	  (setq sgb_lifetime 0)    ;; disappear next tick
	  (if (eq bx nil)
	      (add_object EXPLODE3 (- (x) (random 5)) (- (y) (random 5)) 0)
	    (progn
	      (add_object EXPLODE6 (- (x) (random 5)) (- (y) (random 5)) 0)
	      (do_damage sgb_dam bx (* (cos sgb_angle) 10) (* (sin sgb_angle) 10))))))
      T)))

(def_char LASR_BULLET
  (vars sgb_speed sgb_angle sgb_lastx sgb_lasty
	sgb_bright_color sgb_medium_color sgb_lifetime sgb_dam)
  (funs (ai_fun   las_ai)
	(user_fun sgun_ufun)
	(draw_fun sgun_draw))
  (range 10000 10000)
  (flags (unlistable T)
	 (add_front T))
  (states "art/misc.spe" (stopped  "sgun_bullet")))
