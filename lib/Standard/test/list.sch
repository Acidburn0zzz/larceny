; Tests for lib/list.sch
; 2003-01-01 / lth
;
; FIXME: insert-ordered
; FIXME: remove-duplicates, filter, find, reduce, reduce-right, 
;        fold-left, fold-right.

(require 'list)

(define (fail token . more)
  (display "Error: test failed: ")
  (display token)
  (newline)
  #f)

(define (pairs-of-list l)
  (let loop ((l l) (m '()))
    (if (null? l) 
        (reverse m)
        (loop (cdr l) (cons l m)))))

(or (equal? (cons* 1 2 3) '(1 2 . 3))
    (fail 'cons*:1))
(or (equal? (cons* 1 2 '(3 4 5)) '(1 2 3 4 5))
    (fail 'cons*:2))
(or (equal? (cons* 1) 1)
    (fail 'cons*:3))

(or (let* ((l (list 1 2 3 4))
           (p (pairs-of-list l)))
      (list-set! l 1 37)
      (and (equal? l '(1 37 3 4))
           (every? eq? p (pairs-of-list l))))
    (fail 'list-set!:1))

(or (equal? (list-head '(1 2 3 4 5) 0) '())
    (fail 'list-head:1))
(or (equal? (list-head '(1 2 3 4 5) 2) '(1 2))
    (fail 'list-head:2))
(or (equal? (list-head '(1 2 3 4 5) 5) '(1 2 3 4 5))
    (fail 'list-head:3))

(or (equal? (list-insert '+ '()) '())
    (fail 'list-insert:1))
(or (equal? (list-insert '+ '(1 2 3)) '(1 + 2 + 3))
    (fail 'list-insert:2))

(or (equal? (accumulate eq? '()) '())
    (fail 'accumulate:1))
(or (equal? (accumulate eq? '(a)) '((a)))
    (fail 'accumulate:2))
(or (equal? (accumulate = '(1 1 2 4 4 2)) '((1 1) (2) (4 4) (2)))
    (fail 'accumulate:3))

(or (equal? (accumulate-unordered eq? '()) '())
    (fail 'accumulate-unordered:1))
(or (equal? (accumulate-unordered eq? '(a)) '((a)))
    (fail 'accumulate-unordered:2))
(or (equal? (accumulate-unordered = '(1 1 2 4 4 2)) '((1 1) (2 2) (4 4)))
    (fail 'accumulate-unordered:3))

(or (equal? (filter-map (lambda (x) 37) '()) '())
    (fail 'filter-map:1))
(or (equal? (filter-map = '(1 2 3) '(2 3 4)) '())
    (fail 'filter-map:2))
(or (equal? (filter-map = '(1 2 3) '(1 2 3)) '(#t #t #t))
    (fail 'filter-map:3))

(or (equal? (mappend list '()) '())
    (fail 'mappend:1))
(or (equal? (mappend list '(1 2 3)) '(1 2 3))
    (fail 'mappend:2))
(or (equal? (mappend list '(1 2 3) '(4 5 6)) '(1 4 2 5 3 6))
    (fail 'mappend:3))

(or (equal? (flatten '()) '())
    (fail 'flatten:1))
(or (equal? (flatten '(a b c)) '(a b c))
    (fail 'flatten:2))
(or (equal? (flatten '(a b (c (d e) f) (g h) i)) '(a b c d e f g h i))
    (fail 'flatten:3))

(or (equal? (iota 0) '())
    (fail 'iota:1))
(or (equal? (iota 3) '(0 1 2))
    (fail 'iota:2))

(or (equal? (iota1 0) '())
    (fail 'iota1:1))
(or (equal? (iota1 1) '(1))
    (fail 'iota1:2))
(or (equal? (iota1 4) '(1 2 3 4))
    (fail 'iota1:3))

; Test fails when it should not with low probability if call to
; randomize-list returns a list in the same order.  The longer the
; list the smaller the probability.

(or (let* ((l (iota 100))
	   (ll (randomize-list l)))
      (and (list? ll)
	   (= (length ll) (length l))
	   (every? (lambda (x) (memv x l)) ll)
	   (every? (lambda (x) (memv x ll)) l)
	   (not (equal? l ll))))
    (fail 'randomize-list:1))
       
(or (equal? (map-leaves (lambda (x) (+ x 1)) '()) '())
    (fail 'map-leaves:1))
(or (equal? (map-leaves (lambda (x) (+ x 1)) '(0 (1 (2)))) '(1 (2 (3))))
    (fail 'map-leaves:2))
(or (equal? (map-leaves (lambda (x) (if (null? x) #f (+ x 1)))
                        '((0 ()) (1 (2 . ()))))
            '((1 #f) (2 (3 . ()))))
    (fail 'map-leaves:3))

(or (let* ((v    '#(d e f))
           (tree (list 'a (list 'b 'c v) 'g))
           (x    (tree-copy tree)))
      (and (equal? x '(a (b c #(d e f)) g))
           (not (eq? x tree))
           (not (eq? (cdr x) (cdr tree)))
           (not (eq? (cddr x) (cddr tree)))
           (not (eq? (cdr (cadr x)) (cdr (cadr tree))))
           (not (eq? (cddr (cadr x)) (cddr (cadr tree))))
           (eq? (caddr (cadr x)) v)))
    (fail 'tree-copy:1))

(or (equal? (list-head (make-circular (iota 3)) 5)
            '(0 1 2 0 1))
    (fail 'make-circular:1))

(or (equal? (list-head (make-circular! (iota 3)) 5)
            '(0 1 2 0 1))
    (fail 'make-circular!:1))

(or (equal? (merge < (lambda (x y) x) '(() ())) '())
    (fail 'merge:1))
(or (equal? (merge < (lambda (x y) x) '(() (1 2 3))) '(1 2 3))
    (fail 'merge:2))
(or (equal? (merge < (lambda (x y) x) '((1 2 3) ())) '(1 2 3))
    (fail 'merge:3))
(or (equal? (merge < (lambda (x y) x) '((1 3 4 6 7 9) (2 5 8 10)))
            '(1 2 3 4 5 6 7 8 9 10))
    (fail 'merge:4))
(or (equal? (merge (lambda (x y) (< (car x) (car y)))
                   (lambda (x y) (cons (car x) (cdr y)))
                   '(((1 . 1) (3 . 1) (4 . 1) (6 . 1))
                     ((2 . 2) (3 . 2) (5 . 2) (6 . 2))))
            '((1 . 1) (2 . 2) (3 . 2) (4 . 1) (5 . 2) (6 . 2)))
    (fail 'merge:5))

(let ((x (map exact->inexact '(0 1 2 3 4)))
      (y '(a b c d e))
      (z '((a a) (b b) (c c) (d d) (e e))))
  (or (eq? (sublist-matchq '(b c) y) (cdr y))
      (fail 'sublist-matchq:1))
  (or (eq? (sublist-matchq '(b d) y) #f)
      (fail 'sublist-matchq:2))
  (or (eq? (sublist-matchq '(e f) y) #f)
      (fail 'sublist-matchq:3))
  (or (eq? (sublist-matchv '(1.0 2.0) x) (cdr x))
      (fail 'sublist-matchv:1))
  (or (eq? (sublist-matchv '(1.0 3.0) x) #f)
      (fail 'sublist-matchv:2))
  (or (eq? (sublist-matchv '(4.0 5.0) y) #f)
      (fail 'sublist-matchv:3))
  (or (eq? (sublist-match '((b b) (c c)) z) (cdr z))
      (fail 'sublist-match:1))
  (or (eq? (sublist-match '((b b) (d d)) z) #f)
      (fail 'sublist-match:2))
  (or (eq? (sublist-match '((e e) (f f)) y) #f)
      (fail 'sublist-match:3)))

; eof
