;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;
;;; Private, not exported.
;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(cond-expand
 (larceny

  ;; FIXME: Larceny-specific code for visualization of flonums.
  ;; Assumes IEEE double precision, Larceny's usual representation,
  ;; and little-endian.

  (define (show x)
    (map (lambda (i) (bytevector-like-ref x i))
         '(4 5 6 7 8 9 10 11)))

  (define (show-sign x)
    (bitwise-arithmetic-shift (list-ref (show x) 7) -7))

  (define (show-exponent x)
    (bitwise-ior
     (bitwise-arithmetic-shift (bitwise-and (list-ref (show x) 7) 127)
                               3)
     (bitwise-arithmetic-shift (bitwise-and (list-ref (show x) 6) #b11100000)
                               -5)))

  (define (show-significand x)
    (let ((bytes (show x)))
      (+ (* (list-ref bytes 0) 1)
         (* (list-ref bytes 1) 256)
         (* (list-ref bytes 2) 256 256)
         (* (list-ref bytes 3) 256 256 256)
         (* (list-ref bytes 4) 256 256 256 256)
         (* (list-ref bytes 5) 256 256 256 256 256)
         (* (bitwise-and (list-ref bytes 6) #b00011111)
            256 256 256 256 256 256))))

  )
 (else))


;;; Private but portable code.

(define FIXME 'FIXME)

(define precision-bits    ; IEEE double has 53 bits of precision
  (let loop ((bits 0)
             (x 1.0))
    (if (= x (+ x 1.0))
        bits
        (loop (+ bits 1)
              (* 2.0 x)))))

(define (check-flonum! name x)
  (if (not (flonum? x))
      (error (string-append "non-flonum argument passed to "
                            (symbol->string name))
             x)))

;;; Given a symbol naming a flonum procedure and a generic operation,
;;; returns a flonum procedure that restricts the generic operation
;;; to flonum arguments and result.

(define (flop1 name op)
  (lambda (x)
    (check-flonum! name x)
    (let ((result (op x)))
      (if (not (flonum? result))
          (error (string-append "non-flonum result from "
                              (symbol->string name))
                              result))
      result)))

(define (flop2 name op)
  (lambda (x y)
    (check-flonum! name x)
    (check-flonum! name y)
    (let ((result (op x y)))
      (if (not (flonum? result))
          (error (string-append "non-flonum result from "
                                (symbol->string name))
                                result))
      result)))

(define (flop3 name op)
  (lambda (x y z)
    (check-flonum! name x)
    (check-flonum! name y)
    (check-flonum! name z)
    (let ((result (op x y z)))
      (if (not (flonum? result))
          (error (string-append "non-flonum result from "
                                (symbol->string name))
                                result))
      result)))

;;; Given a flonum x and a list of flonum coefficients for a polynomial,
;;; in order of increasing degree, returns the value of the polynomial at x.

(define (polynomial-at x coefs)
  (if (null? coefs)
      0.0
      (fl+ (car coefs)
           (fl* x (polynomial-at x (cdr coefs))))))

;;; Given a exact non-negative integer, returns its factorial.

(define (fact x)
  (if (zero? x)
      1
      (* x (fact (- x 1)))))

;;; Given a non-negative integral flonum x, returns its factorial.

(define (factorial x)
  (if (flzero? x)
      1.0
      (fl* x (factorial (fl- x 1.0)))))

; eof