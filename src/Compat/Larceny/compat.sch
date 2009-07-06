; Copyright 1998 Lars T Hansen.
;
; $Id$
;
; Larceny -- compatibility library for Twobit running under Larceny.

;; When *exit-on-error* is set, make our error handler die loudly
(cond (*exit-on-error*
       (error-handler 
	(lambda l 
	  (display l)
	  (newline)
	  (exit 114)))))

(define ($$trace x) #t)

(define host-system 'larceny)

(define (write-byte bytenum . rest)
  (let ((port (if (null? rest) 
                  (current-output-port)
                  (car rest))))
    (write-char (integer->char bytenum) port)))

; Temporary?

(define (.check! flag exn . args)
  (if (not flag)
      (apply error "Runtime check exception: " exn args)))

; Temporary until all versions of Larceny have migrated to the new
; environment system.

(call-with-error-handler
 (lambda args
   (set! environment-syntax-environment
         (lambda (env)
           usual-syntactic-environment)))
 (lambda ()
   environment-syntax-environment))

; The compatibility library loads Auxlib if compat:initialize is called
; without arguments.  Compat:load will load fasl files when appropriate.

(define (compat:initialize . rest)
  (if (null? rest)
      (let ((dir (nbuild-parameter 'compatibility))
            (auxlib (nbuild-parameter 'auxiliary)))
	(compat:load (string-append dir "compat2.sch"))
	(compat:load (string-append auxlib "list.sch"))
	(compat:load (string-append auxlib "pp.sch"))
        )))

(define (with-optimization level thunk) 
  (thunk))

; Calls thunk1, and if thunk1 causes an error to be signalled, calls thunk2.

(define (call-with-error-control thunk1 thunk2) 
  (let ((eh (error-handler)))
    (error-handler (lambda args
		     (error-handler eh)
                     (parameterize ((print-length 7)
                                    (print-level 7))
                       (decode-error args))
		     (thunk2)
		     (apply eh args)))
    (thunk1)
    (error-handler eh)))

(define (larc-new-extension fn ext)
  (let* ((l (string-length fn))
	 (x (let loop ((i (- l 1)))
	      (cond ((< i 0) #f)
		    ((char=? (string-ref fn i) #\.) (+ i 1))
		    (else (loop (- i 1)))))))
    (if (not x)
	(string-append fn "." ext)
	(string-append (substring fn 0 x) ext))))

(define (compat:load filename)
  (define (loadit fn)
    (if (nbuild-parameter 'verbose-load?)
	(format #t "~a~%" fn))
    (load fn))
  (if (nbuild-parameter 'always-source?)
      (loadit filename)
      (let ((fn (larc-new-extension filename "fasl")))
	(if (and (file-exists? fn)
		 (compat:file-newer? fn filename))
	    (loadit fn)
	    (loadit filename)))))

(define (compat:file-newer? a b)
  (let* ((ta    (file-modification-time a))
	 (tb    (file-modification-time b))
	 (limit (vector-length ta)))
    (let loop ((i 0))
      (cond ((= i limit)
	     #t)
	    ((= (vector-ref ta i) (vector-ref tb i))
	     (loop (+ i 1)))
	    (else
	     (>= (vector-ref ta i) (vector-ref tb i)))))))

;; "define" recognize-keywords? parameter only if it isn't already
;; defined.  Note that this "definition" of the parameter is just a
;; stub (for backwards compatibility when bootstrapping on older
;; versions of Larceny); the parameter cannot be set in any meaningful
;; way with this definition.
;; Doing the same trick to handle recognize-javadot-symbols? on older
;; versions of Larceny.
(let ((env1 (interaction-environment)))
  (if (not (environment-variable? env1 'recognize-keywords?))
      (set! recognize-keywords? (lambda l #f)))
  (if (not (environment-variable? env1 'recognize-javadot-symbols?))
      (set! recognize-javadot-symbols? (lambda l #f))))

;; Doing the same trick to handle IO on older versions of Larceny
(let ((env1 (interaction-environment)))
  (if (not (environment-variable? env1 'call-with-raw-latin-1-output-file))
      (set! call-with-raw-latin-1-output-file call-with-output-file))
  (if (not (environment-variable? env1 'call-with-raw-latin-1-input-file))
      (set! call-with-raw-latin-1-input-file call-with-input-file))
  (if (not (environment-variable? env1 'open-raw-latin-1-output-file))
      (set! open-raw-latin-1-output-file open-output-file))
  (if (not (environment-variable? env1 'open-raw-latin-1-input-file))
      (set! open-raw-latin-1-input-file open-input-file))
  (if (not (environment-variable? env1 'with-output-to-raw-latin-1-file))
      (set! with-output-to-raw-latin-1-file with-output-to-file))
  (if (not (environment-variable? env1 'with-input-from-raw-latin-1-file))
      (set! with-input-from-raw-latin-1-file with-input-from-file)))

;; Doing the same trick to handle fx* on older
;; versions of Larceny.
(let ((env1 (interaction-environment)))
  (let-syntax ((establish! (syntax-rules () 
                             ((establish name alias)
                              (if (not (environment-variable? env1 'name))
                                  (set! name alias))))))
    (establish! fxlognot lognot)
    (establish! fxlogand logand)
    (establish! fxlogior logior)
    (establish! fxlogxor logxor)
    (establish! fxlsh    lsh)
    (establish! fxrshl   rshl)
    (establish! fxrsha   rsha)
    ))

;; for Sassy
(define (compat:load-sassy)
  (define old-env (interaction-environment))
  (define new-env #f)
  (cond
   ;; Guard to allow bootstrap under old Larceny versions
   ((environment-variable? old-env 'larceny-initialized-environment)
    (set! new-env (environment-copy (larceny-initialized-environment))))
   (else 
    ;; If control goes thru this path, bindings on which SRFI
    ;; libraries rely may have been changed in interaction environment
    ;; (by "changed", I mean "corrupted" -- see Ticket #585)
    (set! new-env (environment-copy old-env))))
  (interaction-environment new-env)
  (for-each 
    require
    '(srfi-0 srfi-1 srfi-9 
	     srfi-23 
	     srfi-56 ;; We may not need 56 (just provide a stub that errors?)
	     srfi-60 srfi-66 srfi-69))

  (for-each (lambda (x)
	      (compat:load (param-filename 'source "Sassy" x)))
   '("inits/larceny-0.97.scm"
     "extras.scm"
     "push-stacks.scm"
     "api.scm"
     "intern.scm"
     "macros.scm"
     "numbers.scm"
     "other/srfi-56-pieces.scm"
     "operands.scm"
     "text-block.scm"
     "opcodes.scm"
     "text.scm"
     "parse.scm"
     "main.scm"
     "flat-bin.scm"
     "elf.scm"))
  (let ((export (lambda (var-sym) 
                  (environment-link-variables! 
                   old-env var-sym new-env var-sym))))
    (for-each 
     export 
     '(sassy 
       sassy-text-bytevector sassy-symbol-table sassy-symbol-offset
       read-byte ;; (for append-file-shell-command-portable)

       ;; from inits/larceny-0.97.scm
       relocs-out-of-range-condition? relocs-out-of-range-labels 

       logand logior lognot hash-table-ref arithmetic-shift)))
  (clear-require-loaded-files!)
  (interaction-environment old-env))

;; Parameter to control reader behavior
(define compat:read-case-sensitive? case-sensitive?)

; eof
