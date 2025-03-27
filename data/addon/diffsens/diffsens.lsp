;; Written by Jonathan Clark, 1995.  All are free to mutilate/copy this addon.


;; this addon must be loaded by other addon, example; in your addon startup
;; fiel, say the following :

;; (if (not (load "addon/diffsens/diffsens.lsp"))
;;    (progn (print ("This package requires the diffsens addon"))    
;;           (quit)))

(def_char DIFFICULTY_SENSOR         ; name of objetc
  (funs (ai_fun diff_sensor_ai)    ; name of function listed below
	(draw_fun dev_draw))       ; dev_draw only draws when in edit mode
                                      ; this makes the object invisiable while playing

  ; this section provides the interface to the level editor    
  (fields ("xvel"  "easy on?    (1=yes, 0=no)") ; recycle variables already defined
	  ("yvel"  "meduim on?  (1=yes, 0=no)")
	  ("xacel" "hard on?    (1=yes, 0=no)")
	  ("yacel" "extreme on? (1=yes, 0=no)"))


  ; this section defines the "visiable" part of the object
  ; we define two states to aid the level designer in telling if the object is on or off
  (states "addon/diffsens/diffsens.spe"  ; filename where state artwork is located
	 (stopped "off")                        ; we will use this as the off state
	 (on_state "on")))


(defun diff_sensor_ai ()            ; no parameters to this function

  (set_state stopped)              ; set animation state to off
  (set_aistate 0)                  ; set aistate to off

  (select difficulty
	  ('easy (if (eq (xvel) 1)
		     (progn
		       (set_aistate 1)           ; set aistate to on
		       (set_state on_state))))   ; set animation state to on
	  ('medium (if (eq (yvel) 1)
		       (progn
			 (set_aistate 1)
			 (set_state on_state))))
	  ('hard (if (eq (xacel) 1)
		     (progn
		       (set_aistate 1)
		       (set_state on_state))))
	  ('extreme (if (eq (yacel) 1)
			(progn
			  (set_aistate 1)
			  (set_state on_state)))))

  T)    ; return true so that we are not deleted


