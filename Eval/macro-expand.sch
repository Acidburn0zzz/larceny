; Copyright 1998 Lars T Hansen.
;
; $Id$
;
; Macro expander interface for the interpreter and the top-level
; environment.  This file is loaded after Twobit's macro expander.

($$trace "macro-expand")

(define *usual-macros*
  (syntactic-copy global-syntactic-environment))

(define *all-macros*
  global-syntactic-environment)


; Used by the interpreter.
;
; The use of *all-macros* in the 'else' case is really wrong -- 
; it is technically an error.  However, it is correct for correct
; code, and it allows std-heap.sch to be used until we fix this
; properly.

(define (interpreter-macro-expand expr . rest)
  (let* ((env
	  (if (null? rest) (interaction-environment) (car rest)))
	 (syntax-env
	  (case (environment-tag env)
	    ((0 1) (syntactic-copy *usual-macros*))
	    ((2)   *all-macros*)
	    (else  *all-macros*))))
    (let ((current-env global-syntactic-environment))
      (dynamic-wind
       (lambda ()
	 (set! global-syntactic-environment syntax-env))
       (lambda ()
	 (macro-expand expr))
       (lambda ()
	 (set! global-syntactic-environment current-env))))))


; Exported to the user environment.

(define (toplevel-macro-expand expr . rest)
  (make-readable (apply interpreter-macro-expand expr rest)))


; `Twobit-sort' is used by Twobit's macro expander.

(define (twobit-sort less? list)
  (sort list less?))

; Generated by macro expander.

(define name:CAR '.car)
(define name:CDR '.cdr)
  
; eof

