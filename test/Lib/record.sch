; Record tests

(define (run-record-tests)
  (display "Record") (newline)
  (record-test-0)
  (record-test-1)
  (record-test-r6rs-0)
  (record-test-r6rs-1)
  (record-test-r6rs-2))

(define (record-test-0)
  (let* ((pt2 (make-record-type "pt2" '(x y)))
         (pt3 (make-record-type "pt3" '(z) pt2)))

    (define a ((record-constructor pt2) 2 3))
    (define b ((record-constructor pt2 '(x)) 2))
    (define c ((record-constructor pt2 '(y)) 3))
    (define d ((record-constructor pt2 '(x y)) 2 3))
    (define e ((record-constructor pt2 '(y x)) 3 2))

    (allof
     (test "((record-accessor pt2 'x) a)" ((record-accessor pt2 'x) a) 2)
     (test "((record-accessor pt2 'y) a)" ((record-accessor pt2 'y) a) 3)
     (test "((record-accessor pt2 'x) b)" ((record-accessor pt2 'x) b) 2)
     (test "((record-accessor pt2 'y) b)" ((record-accessor pt2 'y) b) #f)
     (test "((record-accessor pt2 'x) c)" ((record-accessor pt2 'x) c) #f)
     (test "((record-accessor pt2 'y) c)" ((record-accessor pt2 'y) c) 3)
     (test "((record-accessor pt2 'x) d)" ((record-accessor pt2 'x) d) 2)
     (test "((record-accessor pt2 'y) d)" ((record-accessor pt2 'y) d) 3)
     (test "((record-accessor pt2 'x) e)" ((record-accessor pt2 'x) e) 2)
     (test "((record-accessor pt2 'y) e)" ((record-accessor pt2 'y) e) 3))

    ((record-updater pt2 'x) a 4)
    ((record-updater pt2 'y) a 5)

    (allof
     (test "((record-accessor pt2 'x) a)" ((record-accessor pt2 'x) a) 4)
     (test "((record-accessor pt2 'y) a)" ((record-accessor pt2 'y) a) 5)
     (test "((record-accessor pt2 'x) b)" ((record-accessor pt2 'x) b) 2)
     (test "((record-accessor pt2 'y) b)" ((record-accessor pt2 'y) b) #f)
     (test "((record-accessor pt2 'x) c)" ((record-accessor pt2 'x) c) #f)
     (test "((record-accessor pt2 'y) c)" ((record-accessor pt2 'y) c) 3)
     (test "((record-accessor pt2 'x) d)" ((record-accessor pt2 'x) d) 2)
     (test "((record-accessor pt2 'y) d)" ((record-accessor pt2 'y) d) 3)
     (test "((record-accessor pt2 'x) e)" ((record-accessor pt2 'x) e) 2)
     (test "((record-accessor pt2 'y) e)" ((record-accessor pt2 'y) e) 3))

    (set! a ((record-constructor pt3) 2 3 4))
    (set! b ((record-constructor pt3 '(x)) 2))
    (set! c ((record-constructor pt3 '(y)) 3))
    (set! d ((record-constructor pt3 '(z)) 4))
    (set! e ((record-constructor pt3 '(x y)) 2 3))
    (set! f ((record-constructor pt3 '(z x)) 4 2))

    (allof
     (test "((record-accessor pt2 'x) a)" ((record-accessor pt2 'x) a) 2)
     (test "((record-accessor pt2 'y) a)" ((record-accessor pt2 'y) a) 3)
     (test "((record-accessor pt2 'x) b)" ((record-accessor pt2 'x) b) 2)
     (test "((record-accessor pt2 'y) b)" ((record-accessor pt2 'y) b) #f)
     (test "((record-accessor pt2 'x) c)" ((record-accessor pt2 'x) c) #f)
     (test "((record-accessor pt2 'y) c)" ((record-accessor pt2 'y) c) 3)
     (test "((record-accessor pt2 'x) d)" ((record-accessor pt2 'x) d) #f)
     (test "((record-accessor pt2 'y) d)" ((record-accessor pt2 'y) d) #f)
     (test "((record-accessor pt2 'x) e)" ((record-accessor pt2 'x) e) 2)
     (test "((record-accessor pt2 'y) e)" ((record-accessor pt2 'y) e) 3)
     (test "((record-accessor pt2 'x) f)" ((record-accessor pt2 'x) f) 2)
     (test "((record-accessor pt2 'y) f)" ((record-accessor pt2 'y) f) #f))

    (allof
     (test "((record-accessor pt3 'x) a)" ((record-accessor pt3 'x) a) 2)
     (test "((record-accessor pt3 'y) a)" ((record-accessor pt3 'y) a) 3)
     (test "((record-accessor pt3 'z) a)" ((record-accessor pt3 'z) a) 4)
     (test "((record-accessor pt3 'x) b)" ((record-accessor pt3 'x) b) 2)
     (test "((record-accessor pt3 'y) b)" ((record-accessor pt3 'y) b) #f)
     (test "((record-accessor pt3 'z) b)" ((record-accessor pt3 'z) b) #f)
     (test "((record-accessor pt3 'x) c)" ((record-accessor pt3 'x) c) #f)
     (test "((record-accessor pt3 'y) c)" ((record-accessor pt3 'y) c) 3)
     (test "((record-accessor pt3 'z) c)" ((record-accessor pt3 'z) c) #f)
     (test "((record-accessor pt3 'x) d)" ((record-accessor pt3 'x) d) #f)
     (test "((record-accessor pt3 'y) d)" ((record-accessor pt3 'y) d) #f)
     (test "((record-accessor pt3 'z) d)" ((record-accessor pt3 'z) d) 4)
     (test "((record-accessor pt3 'x) e)" ((record-accessor pt3 'x) e) 2)
     (test "((record-accessor pt3 'y) e)" ((record-accessor pt3 'y) e) 3)
     (test "((record-accessor pt3 'z) e)" ((record-accessor pt3 'z) e) #f)
     (test "((record-accessor pt3 'x) f)" ((record-accessor pt3 'x) f) 2)
     (test "((record-accessor pt3 'y) f)" ((record-accessor pt3 'y) f) #f)
     (test "((record-accessor pt3 'z) f)" ((record-accessor pt3 'z) f) 4))

    ((record-updater pt3 'x) a 5)
    ((record-updater pt3 'y) a 6)
    ((record-updater pt3 'z) a 7)

    ((record-updater pt2 'x) b 5)
    ((record-updater pt2 'y) b 6)

    (allof
     (test "((record-accessor pt2 'x) a)" ((record-accessor pt2 'x) a) 5)
     (test "((record-accessor pt2 'y) a)" ((record-accessor pt2 'y) a) 6)
     (test "((record-accessor pt2 'x) b)" ((record-accessor pt2 'x) b) 5)
     (test "((record-accessor pt2 'y) b)" ((record-accessor pt2 'y) b) 6)
     (test "((record-accessor pt2 'x) c)" ((record-accessor pt2 'x) c) #f)
     (test "((record-accessor pt2 'y) c)" ((record-accessor pt2 'y) c) 3)
     (test "((record-accessor pt2 'x) d)" ((record-accessor pt2 'x) d) #f)
     (test "((record-accessor pt2 'y) d)" ((record-accessor pt2 'y) d) #f)
     (test "((record-accessor pt2 'x) e)" ((record-accessor pt2 'x) e) 2)
     (test "((record-accessor pt2 'y) e)" ((record-accessor pt2 'y) e) 3)
     (test "((record-accessor pt2 'x) f)" ((record-accessor pt2 'x) f) 2)
     (test "((record-accessor pt2 'y) f)" ((record-accessor pt2 'y) f) #f))

    (allof
     (test "((record-accessor pt3 'x) a)" ((record-accessor pt3 'x) a) 5)
     (test "((record-accessor pt3 'y) a)" ((record-accessor pt3 'y) a) 6)
     (test "((record-accessor pt3 'z) a)" ((record-accessor pt3 'z) a) 7)
     (test "((record-accessor pt3 'x) b)" ((record-accessor pt3 'x) b) 5)
     (test "((record-accessor pt3 'y) b)" ((record-accessor pt3 'y) b) 6)
     (test "((record-accessor pt3 'z) b)" ((record-accessor pt3 'z) b) #f)
     (test "((record-accessor pt3 'x) c)" ((record-accessor pt3 'x) c) #f)
     (test "((record-accessor pt3 'y) c)" ((record-accessor pt3 'y) c) 3)
     (test "((record-accessor pt3 'z) c)" ((record-accessor pt3 'z) c) #f)
     (test "((record-accessor pt3 'x) d)" ((record-accessor pt3 'x) d) #f)
     (test "((record-accessor pt3 'y) d)" ((record-accessor pt3 'y) d) #f)
     (test "((record-accessor pt3 'z) d)" ((record-accessor pt3 'z) d) 4)
     (test "((record-accessor pt3 'x) e)" ((record-accessor pt3 'x) e) 2)
     (test "((record-accessor pt3 'y) e)" ((record-accessor pt3 'y) e) 3)
     (test "((record-accessor pt3 'z) e)" ((record-accessor pt3 'z) e) #f)
     (test "((record-accessor pt3 'x) f)" ((record-accessor pt3 'x) f) 2)
     (test "((record-accessor pt3 'y) f)" ((record-accessor pt3 'y) f) #f)
     (test "((record-accessor pt3 'z) f)" ((record-accessor pt3 'z) f) 4))

    (allof
     (test "(record-type-descriptor? pt2)" (record-type-descriptor? pt2) #t)
     (test "(record-type-descriptor? pt3)" (record-type-descriptor? pt3) #t)
     (test "(record-type-descriptor? \"supercallifragilistic\")"
           (record-type-descriptor? "supercallifragilistic") #f)
     (test "(record-type-descriptor? '#(1 2 3))"
           (record-type-descriptor? '#(1 2 3)) #f)
     (test "(record-type-field-names pt2)"
           (record-type-field-names pt2) '(x y))
     (test "(record-type-field-names pt3)"
           (record-type-field-names pt3) '(x y z))
     (test "(record-type-name pt2)" (record-type-name pt2) "pt2")
     (test "(record-type-name pt3)" (record-type-name pt3) "pt3")
     (test "(record-type-extends? pt2 pt2)" (record-type-extends? pt2 pt2) #t)
     (test "(record-type-extends? pt3 pt3)" (record-type-extends? pt3 pt3) #t)
     (test "(record-type-extends? pt2 pt3)" (record-type-extends? pt2 pt3) #f)
     (test "(record-type-extends? pt3 pt2)" (record-type-extends? pt3 pt2) #t)
     (test "(record-type-extends? pt3 (make-record-type \"newpt\" '(z) pt2))"
           (record-type-extends? pt3 (make-record-type "newpt" '(z) pt2))
           #f)
     (test "(record? a)" (record? a) #t)
     (test "(record? '#(a b c))" (record? '#(a b c)) #f)
     (test "(eq? (record-type-descriptor a) pt3)"
           (eq? (record-type-descriptor a) pt3) #t)
     (test "(eq? (record-type-descriptor a) pt2)"
           (eq? (record-type-descriptor a) pt2) #f)
     (test "((record-predicate pt2) a)" ((record-predicate pt2) a) #t)
     (test "((record-predicate pt3) a)" ((record-predicate pt3) a) #t)
     (test "((record-predicate pt3) ((record-constructor pt2) 1 2))"
           ((record-predicate pt3) ((record-constructor pt2) 1 2)) #f))))

; Tests R6RS extensions on old-style record types.

(define (record-test-1)
  (let* ((pt2 (make-record-type "pt2" '(x y)))
         (pt3 (make-record-type "pt3" '(z) pt2)))

    (define a ((record-constructor pt2) 2 3))
    (define b ((record-constructor pt2 '(x)) 2))
    (define c ((record-constructor pt2 '(y)) 3))
    (define d ((record-constructor pt2 '(x y)) 2 3))
    (define e ((record-constructor pt2 '(y x)) 3 2))

    (allof
     (test "(record-type-uid pt3)" (record-type-uid pt3) #f)
     (test "(record-type-sealed? pt3)"
           (record-type-sealed? pt3) #f)
     (test "(record-type-opaque? pt3)"
           (record-type-opaque? pt3) #f)
     (test "(record-field-mutable? pt3 'x)"
           (record-field-mutable? pt3 'x) #t)
     (test "(record-field-mutable? pt3 'y)"
           (record-field-mutable? pt3 'y) #t)
     (test "(record-field-mutable? pt3 'z)"
           (record-field-mutable? pt3 'z) #t)
     (test "(record-field-mutable? pt2 'x)"
           (record-field-mutable? pt2 'x) #t)
     (test "(record-field-mutable? pt2 'y)"
           (record-field-mutable? pt2 'y) #t))))

(define (record-test-r6rs-0)
  (define old-test test)
  (let* ((pt2 (make-record-type-descriptor
               'pt2 #f #f #f #f '#((mutable x) (mutable y))))
         (pt3 (make-record-type-descriptor
               'pt3 pt2 #f #f #f '#((mutable z))))
         (pt3uid (make-record-type-descriptor
                  'pt3 pt2 'uid1234567890 #f #f '#((mutable z))))
         (pt3sealed (make-record-type-descriptor
                     'pt3 pt2 #f #t #f '#((mutable z))))
         (pt3opaque (make-record-type-descriptor
                     'pt3 pt2 #f #f #t '#((mutable z))))
         (pt3immutable (make-record-type-descriptor
                        'pt3 pt2 #f #f #f '#((immutable z)))))

    '
    (define (test name x1 x2)
      (display name) (newline) (old-test name x1 x2))

    (allof
     (test "(record-type-descriptor? pt2)" (record-type-descriptor? pt2) #t)
     (test "(record-type-descriptor? pt3)" (record-type-descriptor? pt3) #t)
     (test "(record-type-descriptor? \"supercallifragilistic\")"
           (record-type-descriptor? "supercallifragilistic") #f)
     (test "(record-type-descriptor? '#(1 2 3))"
           (record-type-descriptor? '#(1 2 3)) #f)
     (test "(record-type-field-names pt2)"
           (record-type-field-names pt2) '#(x y))
     (test "(record-type-field-names pt3)"
           (record-type-field-names pt3) '#(z))
     (test "(record-type-name pt2)" (record-type-name pt2) 'pt2)
     (test "(record-type-name pt3)" (record-type-name pt3) 'pt3)
     (test "(record-type-extends? pt2 pt2)" (record-type-extends? pt2 pt2) #t)
     (test "(record-type-extends? pt3 pt3)" (record-type-extends? pt3 pt3) #t)
     (test "(record-type-extends? pt2 pt3)" (record-type-extends? pt2 pt3) #f)
     (test "(record-type-extends? pt3 pt2)" (record-type-extends? pt3 pt2) #t)
     (test "(record-type-extends? pt3 pt3a)"
           (record-type-extends? pt3
            (make-record-type-descriptor 'pt3 pt2 #f #f #f '#((mutable z))))
           #f)
     (test "(record? pt2)" (record? pt2) #t)
     (test "(record? '#(a b c))" (record? '#(a b c)) #f)

     (test "(record-type-uid pt3)" (record-type-uid pt3) #f)
     (test "(record-type-uid pt3uid)" (record-type-uid pt3uid) 'uid1234567890)
     (test "(record-type-sealed? pt3)"
           (record-type-sealed? pt3) #f)
     (test "(record-type-sealed? pt3sealed)"
           (record-type-sealed? pt3sealed) #t)
     (test "(record-type-opaque? pt3)"
           (record-type-opaque? pt3) #f)
     (test "(record-type-opaque? pt3opaque)"
           (record-type-opaque? pt3opaque) #t)
     (test "(record-field-mutable? pt3 0)"
           (record-field-mutable? pt3 0) #t)
     (test "(record-field-mutable? pt3immutable 0)"
           (record-field-mutable? pt3immutable 0) #f))))

; Tests old-style constructors, accessors, mutators on R6RS record types.a

(define (record-test-r6rs-1)
  (define old-test test)
  (let* ((pt2 (make-record-type-descriptor
               'pt2 #f #f #f #f '#((mutable x) (mutable y))))
         (pt3 (make-record-type-descriptor
               'pt3 pt2 #f #f #f '#((mutable z))))
         (pt3uid (make-record-type-descriptor
                  'pt3 pt2 'uid1234567890 #f #f '#((mutable z))))
         (pt3sealed (make-record-type-descriptor
                     'pt3 pt2 #f #t #f '#((mutable z))))
         (pt3opaque (make-record-type-descriptor
                     'pt3 pt2 #f #f #t '#((mutable z))))
         (pt3immutable (make-record-type-descriptor
                        'pt3 pt2 #f #f #f '#((immutable z))))

         (pt2-cd (make-record-constructor-descriptor pt2 #f #f))
         (pt3-cd (make-record-constructor-descriptor pt3 #f #f))
         (pt3uid-cd (make-record-constructor-descriptor pt3uid #f #f))
         (pt3sealed-cd (make-record-constructor-descriptor pt3sealed #f #f))
         (pt3opaque-cd (make-record-constructor-descriptor pt3opaque #f #f))
         (pt3imm-cd (make-record-constructor-descriptor pt3immutable #f #f))

         (make-pt2 (record-constructor pt2-cd))
         (make-pt3 (record-constructor pt3-cd))
         (make-pt3uid (record-constructor pt3uid-cd))
         (make-pt3sealed (record-constructor pt3sealed-cd))
         (make-pt3opaque (record-constructor pt3opaque-cd))
         (make-pt3imm (record-constructor pt3imm-cd)))

    (define a ((record-constructor pt2) 2 3))
    (define b ((record-constructor pt2 '(x)) 2))
    (define c ((record-constructor pt2 '(y)) 3))
    (define d ((record-constructor pt2 '(x y)) 2 3))
    (define e ((record-constructor pt2 '(y x)) 3 2))

    '
    (define (test name x1 x2)
      (display name) (newline) (old-test name x1 x2))

    (allof
     (test "((record-accessor pt2 'x) a)" ((record-accessor pt2 'x) a) 2)
     (test "((record-accessor pt2 'y) a)" ((record-accessor pt2 'y) a) 3)
     (test "((record-accessor pt2 'x) b)" ((record-accessor pt2 'x) b) 2)
     (test "((record-accessor pt2 'y) b)" ((record-accessor pt2 'y) b) #f)
     (test "((record-accessor pt2 'x) c)" ((record-accessor pt2 'x) c) #f)
     (test "((record-accessor pt2 'y) c)" ((record-accessor pt2 'y) c) 3)
     (test "((record-accessor pt2 'x) d)" ((record-accessor pt2 'x) d) 2)
     (test "((record-accessor pt2 'y) d)" ((record-accessor pt2 'y) d) 3)
     (test "((record-accessor pt2 'x) e)" ((record-accessor pt2 'x) e) 2)
     (test "((record-accessor pt2 'y) e)" ((record-accessor pt2 'y) e) 3))

    ((record-updater pt2 'x) a 4)
    ((record-updater pt2 'y) a 5)

    (allof
     (test "((record-accessor pt2 'x) a)" ((record-accessor pt2 'x) a) 4)
     (test "((record-accessor pt2 'y) a)" ((record-accessor pt2 'y) a) 5)
     (test "((record-accessor pt2 'x) b)" ((record-accessor pt2 'x) b) 2)
     (test "((record-accessor pt2 'y) b)" ((record-accessor pt2 'y) b) #f)
     (test "((record-accessor pt2 'x) c)" ((record-accessor pt2 'x) c) #f)
     (test "((record-accessor pt2 'y) c)" ((record-accessor pt2 'y) c) 3)
     (test "((record-accessor pt2 'x) d)" ((record-accessor pt2 'x) d) 2)
     (test "((record-accessor pt2 'y) d)" ((record-accessor pt2 'y) d) 3)
     (test "((record-accessor pt2 'x) e)" ((record-accessor pt2 'x) e) 2)
     (test "((record-accessor pt2 'y) e)" ((record-accessor pt2 'y) e) 3))

    (set! a ((record-constructor pt3) 2 3 4))
    (set! b ((record-constructor pt3 '(x)) 2))
    (set! c ((record-constructor pt3 '(y)) 3))
    (set! d ((record-constructor pt3 '(z)) 4))
    (set! e ((record-constructor pt3 '(x y)) 2 3))
    (set! f ((record-constructor pt3 '(z x)) 4 2))

    (allof
     (test "((record-accessor pt2 'x) a)" ((record-accessor pt2 'x) a) 2)
     (test "((record-accessor pt2 'y) a)" ((record-accessor pt2 'y) a) 3)
     (test "((record-accessor pt2 'x) b)" ((record-accessor pt2 'x) b) 2)
     (test "((record-accessor pt2 'y) b)" ((record-accessor pt2 'y) b) #f)
     (test "((record-accessor pt2 'x) c)" ((record-accessor pt2 'x) c) #f)
     (test "((record-accessor pt2 'y) c)" ((record-accessor pt2 'y) c) 3)
     (test "((record-accessor pt2 'x) d)" ((record-accessor pt2 'x) d) #f)
     (test "((record-accessor pt2 'y) d)" ((record-accessor pt2 'y) d) #f)
     (test "((record-accessor pt2 'x) e)" ((record-accessor pt2 'x) e) 2)
     (test "((record-accessor pt2 'y) e)" ((record-accessor pt2 'y) e) 3)
     (test "((record-accessor pt2 'x) f)" ((record-accessor pt2 'x) f) 2)
     (test "((record-accessor pt2 'y) f)" ((record-accessor pt2 'y) f) #f))

    (allof
     (test "((record-accessor pt3 'x) a)" ((record-accessor pt3 'x) a) 2)
     (test "((record-accessor pt3 'y) a)" ((record-accessor pt3 'y) a) 3)
     (test "((record-accessor pt3 'z) a)" ((record-accessor pt3 'z) a) 4)
     (test "((record-accessor pt3 'x) b)" ((record-accessor pt3 'x) b) 2)
     (test "((record-accessor pt3 'y) b)" ((record-accessor pt3 'y) b) #f)
     (test "((record-accessor pt3 'z) b)" ((record-accessor pt3 'z) b) #f)
     (test "((record-accessor pt3 'x) c)" ((record-accessor pt3 'x) c) #f)
     (test "((record-accessor pt3 'y) c)" ((record-accessor pt3 'y) c) 3)
     (test "((record-accessor pt3 'z) c)" ((record-accessor pt3 'z) c) #f)
     (test "((record-accessor pt3 'x) d)" ((record-accessor pt3 'x) d) #f)
     (test "((record-accessor pt3 'y) d)" ((record-accessor pt3 'y) d) #f)
     (test "((record-accessor pt3 'z) d)" ((record-accessor pt3 'z) d) 4)
     (test "((record-accessor pt3 'x) e)" ((record-accessor pt3 'x) e) 2)
     (test "((record-accessor pt3 'y) e)" ((record-accessor pt3 'y) e) 3)
     (test "((record-accessor pt3 'z) e)" ((record-accessor pt3 'z) e) #f)
     (test "((record-accessor pt3 'x) f)" ((record-accessor pt3 'x) f) 2)
     (test "((record-accessor pt3 'y) f)" ((record-accessor pt3 'y) f) #f)
     (test "((record-accessor pt3 'z) f)" ((record-accessor pt3 'z) f) 4))

    ((record-updater pt3 'x) a 5)
    ((record-updater pt3 'y) a 6)
    ((record-updater pt3 'z) a 7)

    ((record-updater pt2 'x) b 5)
    ((record-updater pt2 'y) b 6)

    (allof
     (test "((record-accessor pt2 'x) a)" ((record-accessor pt2 'x) a) 5)
     (test "((record-accessor pt2 'y) a)" ((record-accessor pt2 'y) a) 6)
     (test "((record-accessor pt2 'x) b)" ((record-accessor pt2 'x) b) 5)
     (test "((record-accessor pt2 'y) b)" ((record-accessor pt2 'y) b) 6)
     (test "((record-accessor pt2 'x) c)" ((record-accessor pt2 'x) c) #f)
     (test "((record-accessor pt2 'y) c)" ((record-accessor pt2 'y) c) 3)
     (test "((record-accessor pt2 'x) d)" ((record-accessor pt2 'x) d) #f)
     (test "((record-accessor pt2 'y) d)" ((record-accessor pt2 'y) d) #f)
     (test "((record-accessor pt2 'x) e)" ((record-accessor pt2 'x) e) 2)
     (test "((record-accessor pt2 'y) e)" ((record-accessor pt2 'y) e) 3)
     (test "((record-accessor pt2 'x) f)" ((record-accessor pt2 'x) f) 2)
     (test "((record-accessor pt2 'y) f)" ((record-accessor pt2 'y) f) #f))

    (allof
     (test "((record-accessor pt3 'x) a)" ((record-accessor pt3 'x) a) 5)
     (test "((record-accessor pt3 'y) a)" ((record-accessor pt3 'y) a) 6)
     (test "((record-accessor pt3 'z) a)" ((record-accessor pt3 'z) a) 7)
     (test "((record-accessor pt3 'x) b)" ((record-accessor pt3 'x) b) 5)
     (test "((record-accessor pt3 'y) b)" ((record-accessor pt3 'y) b) 6)
     (test "((record-accessor pt3 'z) b)" ((record-accessor pt3 'z) b) #f)
     (test "((record-accessor pt3 'x) c)" ((record-accessor pt3 'x) c) #f)
     (test "((record-accessor pt3 'y) c)" ((record-accessor pt3 'y) c) 3)
     (test "((record-accessor pt3 'z) c)" ((record-accessor pt3 'z) c) #f)
     (test "((record-accessor pt3 'x) d)" ((record-accessor pt3 'x) d) #f)
     (test "((record-accessor pt3 'y) d)" ((record-accessor pt3 'y) d) #f)
     (test "((record-accessor pt3 'z) d)" ((record-accessor pt3 'z) d) 4)
     (test "((record-accessor pt3 'x) e)" ((record-accessor pt3 'x) e) 2)
     (test "((record-accessor pt3 'y) e)" ((record-accessor pt3 'y) e) 3)
     (test "((record-accessor pt3 'z) e)" ((record-accessor pt3 'z) e) #f)
     (test "((record-accessor pt3 'x) f)" ((record-accessor pt3 'x) f) 2)
     (test "((record-accessor pt3 'y) f)" ((record-accessor pt3 'y) f) #f)
     (test "((record-accessor pt3 'z) f)" ((record-accessor pt3 'z) f) 4))

))


; Tests R6RS-style constructors, accessors, mutators on R6RS record types.

(define (record-test-r6rs-2)
  (define old-test test)
  (let* ((pt2 (make-record-type-descriptor
               'pt2 #f #f #f #f '#((mutable x) (mutable y))))
         (pt3 (make-record-type-descriptor
               'pt3 pt2 #f #f #f '#((mutable z))))
         (pt3uid (make-record-type-descriptor
                  'pt3 pt2 'uid1234567890 #f #f '#((mutable z))))
         (pt3sealed (make-record-type-descriptor
                     'pt3 pt2 #f #t #f '#((mutable z))))
         (pt3opaque (make-record-type-descriptor
                     'pt3 pt2 #f #f #t '#((mutable z))))
         (pt3immutable (make-record-type-descriptor
                        'pt3 pt2 #f #f #f '#((immutable z))))

         (pt2-cd (make-record-constructor-descriptor pt2 #f #f))
         (pt3-cd (make-record-constructor-descriptor pt3 #f #f))
         (pt3uid-cd (make-record-constructor-descriptor pt3uid #f #f))
         (pt3sealed-cd (make-record-constructor-descriptor pt3sealed #f #f))
         (pt3opaque-cd (make-record-constructor-descriptor pt3opaque #f #f))
         (pt3imm-cd (make-record-constructor-descriptor pt3immutable #f #f))

         (make-pt2 (record-constructor pt2-cd))
         (make-pt3 (record-constructor pt3-cd))
         (make-pt3uid (record-constructor pt3uid-cd))
         (make-pt3sealed (record-constructor pt3sealed-cd))
         (make-pt3opaque (record-constructor pt3opaque-cd))
         (make-pt3imm (record-constructor pt3imm-cd)))

    (define a (make-pt2 2 3))
    ;(define b ((record-constructor pt2 '(x)) 2))
    ;(define c ((record-constructor pt2 '(y)) 3))
    ;(define d ((record-constructor pt2 '(x y)) 2 3))
    ;(define e ((record-constructor pt2 '(y x)) 3 2))

    '
    (define (test name x1 x2)
      (display name) (newline) (old-test name x1 x2))

    (allof
     (test "((record-accessor pt2 0) a)" ((record-accessor pt2 0) a) 2)
     (test "((record-accessor pt2 1) a)" ((record-accessor pt2 1) a) 3)
    ;(test "((record-accessor pt2 'x) b)" ((record-accessor pt2 'x) b) 2)
    ;(test "((record-accessor pt2 'y) b)" ((record-accessor pt2 'y) b) #f)
    ;(test "((record-accessor pt2 'x) c)" ((record-accessor pt2 'x) c) #f)
    ;(test "((record-accessor pt2 'y) c)" ((record-accessor pt2 'y) c) 3)
    ;(test "((record-accessor pt2 'x) d)" ((record-accessor pt2 'x) d) 2)
    ;(test "((record-accessor pt2 'y) d)" ((record-accessor pt2 'y) d) 3)
    ;(test "((record-accessor pt2 'x) e)" ((record-accessor pt2 'x) e) 2)
    ;(test "((record-accessor pt2 'y) e)" ((record-accessor pt2 'y) e) 3)
)

    ((record-mutator pt2 0) a 4)
    ((record-mutator pt2 1) a 5)

    (allof
     (test "((record-accessor pt2 0) a)" ((record-accessor pt2 0) a) 4)
     (test "((record-accessor pt2 1) a)" ((record-accessor pt2 1) a) 5)
    ;(test "((record-accessor pt2 'x) b)" ((record-accessor pt2 'x) b) 2)
    ;(test "((record-accessor pt2 'y) b)" ((record-accessor pt2 'y) b) #f)
    ;(test "((record-accessor pt2 'x) c)" ((record-accessor pt2 'x) c) #f)
    ;(test "((record-accessor pt2 'y) c)" ((record-accessor pt2 'y) c) 3)
    ;(test "((record-accessor pt2 'x) d)" ((record-accessor pt2 'x) d) 2)
    ;(test "((record-accessor pt2 'y) d)" ((record-accessor pt2 'y) d) 3)
    ;(test "((record-accessor pt2 'x) e)" ((record-accessor pt2 'x) e) 2)
    ;(test "((record-accessor pt2 'y) e)" ((record-accessor pt2 'y) e) 3)
)

    (set! a (make-pt3 2 3 4))
   ;(set! b ((record-constructor pt3 '(x)) 2))
   ;(set! c ((record-constructor pt3 '(y)) 3))
   ;(set! d ((record-constructor pt3 '(z)) 4))
   ;(set! e ((record-constructor pt3 '(x y)) 2 3))
   ;(set! f ((record-constructor pt3 '(z x)) 4 2))

    (allof
     (test "((record-accessor pt2 0) a)" ((record-accessor pt2 0) a) 2)
     (test "((record-accessor pt2 1) a)" ((record-accessor pt2 1) a) 3)
    ;(test "((record-accessor pt2 'x) b)" ((record-accessor pt2 'x) b) 2)
    ;(test "((record-accessor pt2 'y) b)" ((record-accessor pt2 'y) b) #f)
    ;(test "((record-accessor pt2 'x) c)" ((record-accessor pt2 'x) c) #f)
    ;(test "((record-accessor pt2 'y) c)" ((record-accessor pt2 'y) c) 3)
    ;(test "((record-accessor pt2 'x) d)" ((record-accessor pt2 'x) d) #f)
    ;(test "((record-accessor pt2 'y) d)" ((record-accessor pt2 'y) d) #f)
    ;(test "((record-accessor pt2 'x) e)" ((record-accessor pt2 'x) e) 2)
    ;(test "((record-accessor pt2 'y) e)" ((record-accessor pt2 'y) e) 3)
    ;(test "((record-accessor pt2 'x) f)" ((record-accessor pt2 'x) f) 2)
    ;(test "((record-accessor pt2 'y) f)" ((record-accessor pt2 'y) f) #f)
)

    (allof
     (test "((record-accessor pt3 0) a)" ((record-accessor pt3 0) a) 4)
    ;(test "((record-accessor pt3 'x) b)" ((record-accessor pt3 'x) b) 2)
    ;(test "((record-accessor pt3 'y) b)" ((record-accessor pt3 'y) b) #f)
    ;(test "((record-accessor pt3 'z) b)" ((record-accessor pt3 'z) b) #f)
    ;(test "((record-accessor pt3 'x) c)" ((record-accessor pt3 'x) c) #f)
    ;(test "((record-accessor pt3 'y) c)" ((record-accessor pt3 'y) c) 3)
    ;(test "((record-accessor pt3 'z) c)" ((record-accessor pt3 'z) c) #f)
    ;(test "((record-accessor pt3 'x) d)" ((record-accessor pt3 'x) d) #f)
    ;(test "((record-accessor pt3 'y) d)" ((record-accessor pt3 'y) d) #f)
    ;(test "((record-accessor pt3 'z) d)" ((record-accessor pt3 'z) d) 4)
    ;(test "((record-accessor pt3 'x) e)" ((record-accessor pt3 'x) e) 2)
    ;(test "((record-accessor pt3 'y) e)" ((record-accessor pt3 'y) e) 3)
    ;(test "((record-accessor pt3 'z) e)" ((record-accessor pt3 'z) e) #f)
    ;(test "((record-accessor pt3 'x) f)" ((record-accessor pt3 'x) f) 2)
    ;(test "((record-accessor pt3 'y) f)" ((record-accessor pt3 'y) f) #f)
    ;(test "((record-accessor pt3 'z) f)" ((record-accessor pt3 'z) f) 4)
)

    ((record-updater pt3 'x) a 5)
    ((record-updater pt3 'y) a 6)
    ((record-updater pt3 'z) a 7)

   ;((record-updater pt2 'x) b 5)
   ;((record-updater pt2 'y) b 6)

    (allof
     (test "((record-accessor pt2 0) a)" ((record-accessor pt2 0) a) 5)
     (test "((record-accessor pt2 1) a)" ((record-accessor pt2 1) a) 6)
    ;(test "((record-accessor pt2 'x) b)" ((record-accessor pt2 'x) b) 5)
    ;(test "((record-accessor pt2 'y) b)" ((record-accessor pt2 'y) b) 6)
    ;(test "((record-accessor pt2 'x) c)" ((record-accessor pt2 'x) c) #f)
    ;(test "((record-accessor pt2 'y) c)" ((record-accessor pt2 'y) c) 3)
    ;(test "((record-accessor pt2 'x) d)" ((record-accessor pt2 'x) d) #f)
    ;(test "((record-accessor pt2 'y) d)" ((record-accessor pt2 'y) d) #f)
    ;(test "((record-accessor pt2 'x) e)" ((record-accessor pt2 'x) e) 2)
    ;(test "((record-accessor pt2 'y) e)" ((record-accessor pt2 'y) e) 3)
    ;(test "((record-accessor pt2 'x) f)" ((record-accessor pt2 'x) f) 2)
    ;(test "((record-accessor pt2 'y) f)" ((record-accessor pt2 'y) f) #f)
)

    (allof
     (test "((record-accessor pt3 0) a)" ((record-accessor pt3 0) a) 7)
    ;(test "((record-accessor pt3 'x) b)" ((record-accessor pt3 'x) b) 5)
    ;(test "((record-accessor pt3 'y) b)" ((record-accessor pt3 'y) b) 6)
    ;(test "((record-accessor pt3 'z) b)" ((record-accessor pt3 'z) b) #f)
    ;(test "((record-accessor pt3 'x) c)" ((record-accessor pt3 'x) c) #f)
    ;(test "((record-accessor pt3 'y) c)" ((record-accessor pt3 'y) c) 3)
    ;(test "((record-accessor pt3 'z) c)" ((record-accessor pt3 'z) c) #f)
    ;(test "((record-accessor pt3 'x) d)" ((record-accessor pt3 'x) d) #f)
    ;(test "((record-accessor pt3 'y) d)" ((record-accessor pt3 'y) d) #f)
    ;(test "((record-accessor pt3 'z) d)" ((record-accessor pt3 'z) d) 4)
    ;(test "((record-accessor pt3 'x) e)" ((record-accessor pt3 'x) e) 2)
    ;(test "((record-accessor pt3 'y) e)" ((record-accessor pt3 'y) e) 3)
    ;(test "((record-accessor pt3 'z) e)" ((record-accessor pt3 'z) e) #f)
    ;(test "((record-accessor pt3 'x) f)" ((record-accessor pt3 'x) f) 2)
    ;(test "((record-accessor pt3 'y) f)" ((record-accessor pt3 'y) f) #f)
    ;(test "((record-accessor pt3 'z) f)" ((record-accessor pt3 'z) f) 4)
)

))
