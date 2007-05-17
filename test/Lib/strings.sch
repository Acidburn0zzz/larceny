; Copyright 2007 William D Clinger
;
; $Id$
;
; These are ustring tests for now, but will become
; string tests after we have completed the switch
; to Unicode.
;
; These tests are woefully incomplete, testing only
; the primops.  More tests will be added after the
; (r6rs unicode) library has been added to Larceny.

(define (run-string-tests)
  (display "String") (newline)
  (string-predicate-test)
  ;(string-simple-comparisons)
  ;(string-more-simple-comparisons #\a)
  ;(string-tests-for-control)
  ;(string-more-tests-for-control #\a)
  ;(string-yet-more-tests-for-control #\a #\b)
  ;(string-conversion-tests #\a)
  ;(string-classification-tests)
  (basic-unicode-string-tests))

(define (string-predicate-test)
  (allof "string?"
   (test "(string? (make-string 0))" (string? (make-string 0)) #t)
   (test "(string? (make-bytevector 0))" (string? (make-bytevector 0)) #f)
   (test "(string? (make-vector 0))" (string? (make-vector 0)) #f)
   (test "(string? (current-output-port))" (string? (current-output-port)) #f)
   (test "(string? open-input-file)" (string? open-input-file) #f)
   (test "(string? #\a)" (string? #\a) #f)
   (test "(string? 37)" (string? 37) #f)
   (test "(string? #x26)" (string? #x26) #f)
   (test "(string? 0.0)" (string? 0.0) #f)
   (test "(string? #'())" (string? '#()) #f)
   (test "(string? '(a . b))" (string? '(a . b)) #f)
   (test "(string? \"\")" (string? "") #t)
   (test "(string? \"a\")" (string? "a") #t)
   (test "(string? #t)" (string? #t) #f)
   (test "(string? #f)" (string? #f) #f)
   (test "(string? '())" (string? '()) #f)
   (test "(string? (unspecified))" (string? (unspecified)) #f)
   (test "(string? (undefined))" (string? (undefined)) #f)
   ))
  
(define (basic-unicode-string-tests)

  ; FIXME: This goes away when strings replace strings.

  (define (string . chars)
    (let* ((n (length chars))
           (s (make-string n)))
      (do ((i 0 (+ i 1))
           (chars chars (cdr chars)))
          ((= i n) s)
        (string-set! s i (car chars)))))

  (define es-zed (integer->char #x00df))
  (define final-sigma (integer->char #x03c2))
  (define lower-sigma (integer->char #x03c3))
  (define upper-sigma (integer->char #x03a3))
  (define upper-chi (integer->char #x03a7))
  (define upper-alpha (integer->char #x0391))
  (define upper-omicron (integer->char #x039f))
  (define lower-chi (integer->char #x03c7))
  (define lower-alpha (integer->char #x03b1))
  (define lower-omicron (integer->char #x03bf))

  (define null (integer->char 0))
  (define biggy (integer->char #x10ffff))

  (let ()
	
  (define strasse (string #\S #\t #\r #\a es-zed #\e))
  (define upper-chaos (string upper-chi upper-alpha upper-omicron upper-sigma))
  (define final-chaos (string lower-chi lower-alpha lower-omicron final-sigma))
  (define lower-chaos (string lower-chi lower-alpha lower-omicron lower-sigma))

  (test "(string-length (make-string 0))"
        (string-length (make-string 0)) 0)
  (test "(string-length (make-string 34))"
        (string-length (make-string 34)) 34)
  (test "(string-length strasse)" (string-length strasse) 6)

  (test "(string-ref (make-string 5 #\w) 0)"
        (string-ref (make-string 5 #\w) 0) #\w)
  (test "(string-ref (make-string 5 #\w) 4)"
        (string-ref (make-string 5 #\w) 4) #\w)
  (test "(string-ref strasse 0)" (string-ref strasse 0) #\S)
  (test "(string-ref strasse 1)" (string-ref strasse 1) #\t)
  (test "(string-ref strasse 2)" (string-ref strasse 2) #\r)
  (test "(string-ref strasse 3)" (string-ref strasse 3) #\a)
  (test "(string-ref strasse 4)" (string-ref strasse 4) #\x00df)
  (test "(string-ref strasse 5)" (string-ref strasse 5) #\e)
  (test "(string-ref upper-chaos 3)" (string-ref upper-chaos 3) upper-sigma)
  (test "(string-ref final-chaos 3)" (string-ref final-chaos 3) final-sigma)
  (test "(string-ref lower-chaos 3)" (string-ref lower-chaos 3) lower-sigma)

  (test "(string-set! lower-chaos 0 #\nul)"
        (begin (string-set! lower-chaos 0 null)
               (string-ref lower-chaos 0))
        null)
  (test "(string-set! lower-chaos 3 biggy)"
        (begin (string-set! lower-chaos 3 biggy)
               (string-ref lower-chaos 3))
        biggy)
  (test "(string->list lower-chaos)"
        (list (string-ref lower-chaos 0)
              (string-ref lower-chaos 1)
              (string-ref lower-chaos 2)
              (string-ref lower-chaos 3))
        (list #\x0 lower-alpha lower-omicron biggy))))
    
; eof
