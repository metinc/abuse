;; Written by Jonathan Clark, 1995.  All are free to mutilate/copy this addon.

;; this addon must be loaded by other addon, example; in your addon startup
;; file, say the following :

;; (if (not (load "addon/nextlev2/nextlev2.lsp"))
;;    (progn (print ("This package requires the diffsens addon"))    
;;           (quit)))

;; or you can just grab this text and stick it at the start of your lisp startup file
;; (this probably prefered because then you don't have to worry about someone having it
;; or not).


(setq base_level_dir "addon/nextlev2/")   ; replace this string with the directory where your levels reside

;; The following is a list of levels to lead depending on the difficulty the player is on
;; if you want, you can set them all to the same.  This example uses a sseperate list for
;; easy and extreme

;; You must have hardness.lsp loaded before this addon!  Hardness.lsp is loaded by startup.lsp
;; which is loaded by abuse.lsp so you can do something like the following :

;; (load "abuse.lsp")
;; (load "addon/myaddon/myaddon1.lsp")
;; (load "addon/myaddon/myaddon2.lsp")
;; (load "addon/nextlev2.lsp")


(perm-space)   ;;;; we are going to set some global variables so do it in "perm-space"

(defun get_level_list () 
  (select difficulty
	  ('easy    '("myeasy1.spe" "myeasy2.spe" "myeasy3.spe"))
	  ('medium  '("myeasy1.spe" "myeasy2.spe" "myeasy3.spe"))
	  ('hard    '("myeasy1.spe" "myeasy2.spe" "myeasy3.spe"))
	  ('extreme '("mybad1.spe"  "mybad2.spe"  "mybad3.spe" ))))


(setq current_level (get_level_list))


;; set the first level
(set_first_level (concatenate 'string base_level_dir (car current_level)))

;; advace the list to point to the next level
(setq current_level (cdr current_level))



(defun next_level2_ai ()
  (perm-space)     ; because we are modifing global vars we need to do so in "perm-space"

  (if (and (touching_bg) (with_object (bg) (pressing_action_key)))
      (if (not current_level)  ; no more levels to load
	  (request_end_game)
	(progn
	  (request_level_load (concatenate 'string base_level_dir (car current_level)))
	  (setq current_level (cdr current_level)))))

  (tmp-space)
  T)
	

(def_char NEXT_LEVEL2
  (funs (ai_fun next_level2_ai)) 
  (flags (can_block T))
  (states "art/misc.spe"
	  (stopped "end_port2")))

(tmp-space)
