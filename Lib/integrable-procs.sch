; -*- Scheme -*-
;
; Larceny run-time system
; Global procedure definitions for integrable procedures.
;
; $Id: integrable-procs.scm,v 1.5 1992/08/04 18:27:22 lth Exp $

; The following are defined elsewhere:
;
;  (symbol? x)       in oblist.sch

(define sys$resource-usage!
  (lambda (x)
    (error "The procedure sys$resource-usage is not supported!")))

(define gc
  (lambda (x)
    (error "The procedure gc is not supported!")))

;; exit should also flush output buffers, etc, etc.
(define exit (lambda () (sys$exit)))

(define dumpheap 
  (lambda (filename proc)
    (cond ((not (string? filename))
	   (error "Bad filename argument to dumpheap: " filename))
	  ((not (procedure? proc))
	   (error "Bad procedure argument to dumpheap: " proc))
	  (else
	   (display "Dumping heap...") (newline)
	   (sys$dumpheap filename proc)
	   (display "Done.") (newline)))))

(define break (lambda () (break)))
(define creg (lambda () (creg)))
; (define undefined (lambda () (undefined)))
(define unspecified (lambda () (unspecified)))
(define typetag (lambda (x) (typetag x)))
(define not (lambda (x) (not x)))
(define null? (lambda (x) (null? x)))
(define pair? (lambda (x) (pair? x)))
(define car (lambda (x) (car x)))
(define cdr (lambda (x) (cdr x)))
(define number? (lambda (x) (number? x)))
(define complex? (lambda (x) (complex? x)))
(define real? (lambda (x) (real? x)))
(define rational? (lambda (x) (rational? x)))
(define integer? (lambda (x) (integer? x)))
(define fixnum? (lambda (x) (fixnum? x)))
(define exact? (lambda (x) (exact? x)))
(define inexact? (lambda (x) (inexact? x)))
(define exact->inexact (lambda (x) (exact->inexact x)))
(define inexact->exact (lambda (x) (inexact->exact x)))
(define round (lambda (x) (round x)))
(define truncate (lambda (x) (truncate x)))
(define zero? (lambda (x) (zero? x)))
(define -- (lambda (x) (-- x)))
(define lognot (lambda (x) (lognot x)))
; (define sqrt (lambda (x) (sqrt x)))
(define real-part (lambda (x) (real-part x)))
(define imag-part (lambda (x) (imag-part x)))
(define char? (lambda (x) (char? x)))
(define char->integer (lambda (x) (char->integer x)))
(define integer->char (lambda (x) (integer->char x)))
(define string? (lambda (x) (string? x)))
(define string-length (lambda (x) (string-length x)))
(define vector? (lambda (x) (vector? x)))
(define vector-length (lambda (x) (vector-length x)))
(define bytevector? (lambda (x) (bytevector? x)))
(define bytevector-length (lambda (x) (bytevector-length x)))
(define make-bytevector (lambda (x) (make-bytevector x)))
; (define structure? (lambda (x) (structure? x)))
(define procedure? (lambda (x) (procedure? x)))
(define procedure-length (lambda (x) (procedure-length x)))
(define make-procedure (lambda (x) (make-procedure x)))
(define creg-set! (lambda (x) (creg-set! x)))
(define make-cell (lambda (x) (make-cell x)))
(define cell-ref (lambda (x) (cell-ref x)))
(define typetag-set! (lambda (x y) (typetag-set! x y)))
(define eq? (lambda (x y) (eq? x y)))
(define eqv? (lambda (x y) (eqv? x y)))
(define cons (lambda (x y) (cons x y)))
(define set-car! (lambda (x y) (set-car! x y)))
(define set-cdr! (lambda (x y) (set-cdr! x y)))
(define quotient (lambda (x y) (quotient x y)))
(define logand (lambda (x y) (logand x y)))
(define logior (lambda (x y) (logior x y)))
(define logxor (lambda (x y) (logxor x y)))
(define lsh (lambda (x y) (lsh x y)))
(define rshl (lambda (x y) (rshl x y)))
(define rsha (lambda (x y) (rsha x y)))
; (define rot (lambda (x y) (rot x y)))
(define make-rectangular (lambda (x y) (make-rectangular x y)))
(define string-ref (lambda (x y) (string-ref x y)))
(define vector-ref (lambda (x y) (vector-ref x y)))
(define bytevector-ref (lambda (x y) (bytevector-ref x y)))
(define procedure-ref (lambda (x y) (procedure-ref x y)))
(define cell-set! (lambda (x y) (cell-set! x y)))
(define char<? (lambda (x y) (char<? x y)))
(define char<=? (lambda (x y) (char<=? x y)))
(define char=? (lambda (x y) (char=? x y)))
(define char>? (lambda (x y) (char>? x y)))
(define char>=? (lambda (x y) (char>=? x y)))
(define sys$partial-list->vector (lambda (x y) (sys$partial-list->vector x y)))
(define vector-set! (lambda (x y z) (vector-set! x y z)))
(define bytevector-set! (lambda (x y z) (bytevector-set! x y z)))
(define procedure-set! (lambda (x y z) (procedure-set! x y z)))
(define vector-like? (lambda (x) (vector-like? x)))
(define bytevector-like? (lambda (x) (bytevector-like? x)))
(define vector-like-ref (lambda (x y) (vector-like-ref x y)))
(define bytevector-like-ref (lambda (x y) (bytevector-like-ref x y)))
(define vector-like-set! (lambda (x y z) (vector-like-set! x y z)))
(define bytevector-like-set! (lambda (x y z) (bytevector-like-set! x y z)))
(define vector-like-length (lambda (x) (vector-like-length x)))
(define bytevector-like-length (lambda (x) (bytevector-like-length x)))
(define remainder (lambda (x y) (remainder x y)))
(define modulo (lambda (x y) (modulo x y)))
(define bytevector-fill! (lambda (x y) (bytevector-fill! x y)))

(define +
  (lambda args
    (if (null? args)
	0
	(let loop ((sum (car args)) (args (cdr args)))
	  (if (null? args)
	       sum
	       (loop (+ sum (car args)) (cdr args)))))))

(define - 
  (lambda (arg . args)
    (if (null? args)
	(-- arg)
	(let loop ((n arg) (args args))
	  (if (null? args)
	      n
	      (loop (- n (car args)) (cdr args)))))))

; The following procedures actually take a variable number
; of arguments.  These definitions might do for the moment.
; See note of bug above.

(define * (lambda (x y) (* x y)))
(define / (lambda (x y) (/ x y)))

(define < (lambda (x y) (< x y)))
(define <= (lambda (x y) (<= x y)))
(define = (lambda (x y) (= x y)))
(define > (lambda (x y) (> x y)))
(define >= (lambda (x y) (>= x y)))

(define make-vector (lambda (x y) (make-vector x y)))

; obsolete in v0.20.
;(define debugvsm (lambda () (debugvsm)))
;(define reset (lambda () (sys$reset)))
