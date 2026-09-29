;; Final-level machine, shared by single-player and co-op startup scripts.

(defun end_game_ai ()
  (if (activated)
      (if (eq (aistate) 8)
	  (if (not (next_picture))
	      (request_end_game))
        (progn
          ;; Save the rotation phase during the initial switch delay. This is
          ;; independent of the local plot setting, including in co-op.
          (set_aitype (mod (game_tick) 9))
          (set_aistate (+ (aistate) 1)))))
  T)


;; Decelerate from one frame per tick to zero over 60 ticks, then stay still.
;; The distance is the sum of speeds 60/60, 59/60, ... measured in frames.
(defun end_game_stop_frame (elapsed phase)
  (let ((ticks (if (< elapsed 60) elapsed 60)))
    (mod (+ phase (/ (* ticks (- 121 ticks)) 120)) 9)))


(defun end_game_draw ()
  (if original_plot
      (let ((saved_state (state))
            (saved_frame (current_frame)))
        ;; Only change the displayed sprite. The shared AI timeline still
        ;; decides when to finish, regardless of each co-op player's plot.
        (set_state running)
        (set_current_frame
          (if (eq (aistate) 0)
              (mod (game_tick) 9)
            (if (< (aistate) 8)
                (aitype)
              (end_game_stop_frame saved_frame (aitype)))))
        (draw)
        (set_state saved_state)
        (set_current_frame saved_frame))
    (draw)))


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
  (funs (ai_fun end_game_ai)
        (draw_fun end_game_draw))
  (range 0 0)
  (states "art/fore/endgame.spe"
          ;; Nine loops, accelerating from quarter speed to full speed over
          ;; 45 ticks (about 3 seconds). The animation lasts 99 ticks (~6.4 s).
          (stopped (let ((frames (seq "pipe" 1 9)))
                     (let ((three_loops (app frames (app frames frames))))
                       (end_game_spin_frames
                         (app three_loops (app three_loops three_loops))
                         15 0))))
          (running (seq "pipe" 1 9))))
