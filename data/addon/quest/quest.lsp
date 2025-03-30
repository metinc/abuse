(perm-space)

;; this is a simple check to see if they player has an engine version
;; capable of playing the game.  All games should at least check for version 1.0
;; because all version before that are beta and have known bugs.
(if (< (+ (* (major_version) 100) (minor_version)) 100)    ; require at least version 1.0
    (progn
      (print "Your engine is out of date.  This game requires version 1.0")
      (quit)))


(setq quest_dir "addon/quest/")  ; in case we change the location of these files later
				 ; this is always a very good idea to do because the user of
                                 ; this program may/may not be able to install into this directory 

; get the proper directory for a file
(defun quest_file (filename) (concatenate 'string quest_dir filename))


;(setq load_warn nil)            ; don't show a waringing if these files aren't there
(load "lisp/english.lsp")       ; need this for various translated messages (english only pong for now!)
(load "gamma.lsp")              ; gamma correction values (if saved)
;(setq load_warn T)

(load "addon/pong/common.lsp")        ; grab the definition of abuse's light holder & obj mover
(load "addon/pong/userfuns.lsp")      ; load seq defun
(load "lisp/input.lsp")         ; get input mapping stuff from abuse


;; these are a few things that the engine requires you to load...
(load_big_font     "art/letters.spe" "letters")
(load_small_font   "art/letters.spe" "small_font")
(load_console_font "art/consfnt.spe" "fnt5x7")
(load_color_filter "art/back/backgrnd.spe")
(load_palette      "art/back/backgrnd.spe")
(load_tiles (quest_file "quest.spe"))  ; load all foreground & background type images from pong.spe

;; this is the image that will be displayed when the game starts
;; this needs to be in the form (X . Y) where X is the filename and
;; Y is the name of the image
(setq title_screen      (cons (quest_file "quest.spe") "title_screen"))


(def_char START
  (funs (draw_fun dev_draw)   ; dev draw is a compiled fun
	(ai_fun do_nothing))  ; always return T, therefore it never "dies"
  (states (quest_file "quest.spe") (stopped "start")))


(defun player_mover (xm ym but)
  (if (> xm 0)
      (try_move speed 0 1)
    (if (< xm 0)
	(try_move (- 0 speed) 0 1)))
  (if (> ym 0)
      (try_move 0 speed 1)
    (if (< ym 0)
	(try_move 0 (- 0 speed) 1)))
  0)

(defun player_cons ()
  (setq speed 5))

(def_char PLAYER
  (vars speed)
  (funs (move_fun player_mover)    ; move fun get's passed the player input and responsible for calling ai_fun
	(constructor player_cons))
  (abilities (walk_top_speed 2)
	     (start_accel 2))
  (flags (can_block T))
  (states (quest_file "quest.spe") (stopped  "player")))


(create_players PLAYER)
(set_first_level (quest_file "main_map.spe"))
(gc)    ; garbage collect 
(tmp-space)

