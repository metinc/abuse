;; Final-level machine, shared by single-player and co-op startup scripts.

(defun end_game_ai ()
  (if (activated)
      (if (eq (aistate) 8)
	  (if (not (next_picture))
	      (request_end_game))
	(set_aistate (+ (aistate) 1))))
  T)


;; Build the startup animation with speed measured in sixtieths of a frame
;; per tick. Carry fractional progress forward and increase speed every tick,
;; so acceleration continues smoothly across loop boundaries.
(defun end_game_spin_frames (frames speed progress)
  (let ((result nil))
    (do () ((null frames) result)
      (progn
        (setq result (if result
                         (nconc result (list (car frames)))
                       (list (car frames))))
        (setq progress (+ progress speed))
        (if (>= progress 60)
            (progn
              (setq frames (cdr frames))
              (setq progress (- progress 60))))
        (if (< speed 60) (setq speed (+ speed 1)))))))


(def_char END_GAME
  (funs (ai_fun end_game_ai))
  (range 0 0)
  (states "art/fore/endgame.spe"
          ;; Nine loops, accelerating from quarter speed to full speed over
          ;; 45 ticks (about 3 seconds). The animation lasts 99 ticks (~6.4 s).
          (stopped (let ((frames (seq "pipe" 1 9)))
                     (let ((three_loops (app frames (app frames frames))))
                       (end_game_spin_frames
                         (app three_loops (app three_loops three_loops))
                         15 0))))))
