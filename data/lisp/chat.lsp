(enable_chatting)

(defun chat_input (str)
  (if (and (> (length str) 0) (equal (elt str 0) #\/))
      (if (search "/help" str)
	  (if (local_player)
	      (chat_print "Commands : /help"))
	(if (local_player)
	    (chat_print (concatenate 'string "unknown command " str))))

    (chat_print (concatenate 'string (player_name) ": " str))))
