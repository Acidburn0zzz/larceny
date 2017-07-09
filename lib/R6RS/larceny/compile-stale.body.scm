;;; FIXME: unfinished; computes library dependencies, but doesn't compile

;;; Given no arguments, re-compiles all libraries and programs
;;; found within the current directory, provided their names end
;;; with .sld, .sls, or .sps
;;;
;;; Given strings naming files within the current directory that
;;; contain libraries or programs to be compiled, compiles those
;;; files if it is safe to do or generates a warning message that
;;; explains the compilation might be unsafe.
;;; Returns true iff all of the given files are compiled.
;;;
;;; It is safe to compile the given files iff, for them and for
;;; each of the libraries on which they depend, the library
;;;
;;;     has already been compiled and is not stale
;;;  or resides within the current directory
;;;         and no library or program outside of the current directory
;;;             depends on it
;;;
;;; Dependency is transitive, so the second of those conditions
;;; implies that no library or program outside of the current
;;; directory depends on any files within the current directory
;;; that will be compiled by compile-stale.
;;;
;;; FIXME: the dependency graph is not perfectly reliable because
;;;     it is unsafe to expand macros during calculation of dependencies
;;;     the graph is calculated using cond-expand features recognized
;;;         when compile-stale is called, which may be different from
;;;         cond-expand features recognized at run time
;;;     the graph is calculated by looking only at import declarations
;;;         at the head of a library or program (FIXME)
;;;     the graph is calculated without expanding cond-expand (FIXME)

(define (compile-stale . files)
  (%compile-stale-libraries files))

;;; Given a list of file names containing R7RS/R6RS libraries or
;;; programs, attempts to compile those files.
;;;
;;; If any of the named files depends upon a library that cannot
;;; be located within the current require path, an error message
;;; will be printed and no files will be compiled.
;;;
;;; If any of the named files depends upon a stale file that lies
;;; outside of the current directory, an error message will be
;;; printed and no files will be compiled.
;;;
;;; In the process of compiling the named files, compile-stale
;;; will also attempt to compile all library source files X
;;; within the current directory such that
;;;
;;;     one of the named files depends upon X, and X has not
;;;     been compiled
;;;
;;;     one of the named files depends upon X, and the compiled
;;;     form of X is stale
;;;
;;;     X depends upon any files that will be compiled as
;;;     a consequence of these rules
;;;
;;; If the list of named files is empty, no files will be compiled.

;;; Algorithm:
;;;     Locate all files upon which the named files depend.
;;;     If any cannot be located, then stop (with error message).
;;;     Locate all stale compiled files upon which the named files depend.
;;;     If any of those stale files lie outside the current directory,
;;;         then stop (with error message).
;;;     Locate all available libraries that depend upon one of the
;;;         named or stale files.
;;;     If any of those libraries lie outside the current directory,
;;;         then stop (with error message).
;;;     The files to be compiled consist of
;;;         the named files
;;;         the stale files upon which a named file depends
;;;         the files upon which a named file depends that depend
;;;             upon a named or stale file
;;;         the files within the current directory that depend upon
;;;             a named or stale files
;;;     Sort the files to be compiled so every file will be compiled
;;;         before all files that depend upon it.
;;;     Compile the files in that order.
;;;     If any compilation fails, restore all of the compiled files
;;;         to their previous state.

(define (%compile-stale-libraries filenames)
  (%compile-stale-libraries-FIXME filenames))

(define (%compile-stale-libraries-FIXME filenames)
  (let* ((dir (current-directory))
         (dirs (current-require-path))
         (dirs (if (member dir dirs) dirs (cons dir dirs))))
    (parameterize ((current-require-path dirs))
     (%compile-stale-libraries1 filenames))))

(define (complain-about-file filename)
  (error 'compile-stale
         "file contains a malformed library or top-level program"
         filename))

;;; larceny:available-source-libraries returns an association list
;;; in which each entry is of the form
;;;
;;;     (<library> <exports> <imports> <filename> <multiple>)
;;;
;;; where
;;;
;;;     <library> is the name of a library
;;;     <exports> is its export form
;;;     <imports> is its import form
;;;     <filename> is the source file that defines the library
;;;     <multiple> is true if two distinct files define the library
;;;
;;; The following procedure computes similar entries from the named
;;; files to be compiled.  For top-level programs, the <library>
;;; will be an empty list.

(define (%compute-entries-for-files filenames)

  (define (okay? keyword form)
    (eq? keyword (car form)))

  (define (%compute-entries-for-port p filename entries)
    (let loop ((library (read p))
               (entries entries))
      (cond ((eof-object? library)
             entries)
            ((not (list? library))
             (complain-about-file filename))
            ((and (<= 4 (length library))
                  (memq (car library) *library-keywords*)
                  (let ((name (cadr library))
                        (exports (caddr library))
                        (imports (cadddr library)))
                    (and (pair? name)
                         (list? name)
                         (okay? (car name) name)
                         (okay? 'export exports)
                         (okay? 'import imports)
                         (let* ((filename (larceny:absolute-path filename)))
                           (loop (read p)
                                 (cons (larceny:make-library-entry
                                        name exports imports filename #f)
                                       entries)))))))
            ((and (<= 1 (length library))
                  (okay? 'import library))
             (let* ((imports library) ; it's really just an import form
                    (filename (larceny:absolute-path filename)))
               (cons (larceny:make-library-entry '()
                                                 '(export)
                                                 imports
                                                 filename
                                                 #f)
                     entries)))
            (else
             (complain-about-file filename)))))

  (if (null? filenames)
      '()
      (let ((filename (car filenames))
            (filenames (cdr filenames)))
        (append (call-with-input-file
                 filename
                 (lambda (p)
                   (%compute-entries-for-port p filename '())))
                (%compute-entries-for-files filenames)))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;; Phase 1: collects dependency information for the files to compile
;;; and for all available libraries, and then proceeds to phase 2.

(define (%compile-stale-libraries1 filenames)
  (let* ((libs (larceny:available-source-libraries))
         (to-compile (%compute-entries-for-files filenames))
         (libs (map (lambda (lib)
                      (cons (larceny:libname-without-version (car lib))
                            (cdr lib)))
                    libs))
         (to-compile (map (lambda (lib)
                            (cons (larceny:libname-without-version (car lib))
                                  (cdr lib)))
                          to-compile))
         (lib-table (make-hashtable equal-hash equal?))
         (comp-table (make-hashtable equal-hash equal?))
         (dependency-table (make-hashtable equal-hash equal?))
         (counter 0))

    ;; Compute comp-table.

    (for-each (lambda (lib/pgm)
                (let* ((name (car lib/pgm))
                       (name (if (null? name)
                                 (begin (set! counter (+ 1 counter))
                                        (list '#(program) counter))
                                 name))
                       (lib/pgm (cons name (cdr lib/pgm))))
                  (hashtable-set! comp-table
                                  (larceny:library-entry-name lib/pgm)
                                  lib/pgm)))
              to-compile)

    ;; Compute lib-table.

    (for-each (lambda (lib)
                (let* ((name (car lib))
                       (probe (hashtable-ref lib-table name #f)))
                  (hashtable-set! lib-table
                                  name
                                  (if (not probe)
                                      lib
                                      (larceny:make-library-entry
                                       name
                                       (larceny:library-entry-exports lib)
                                       (larceny:library-entry-imports lib)
                                       (larceny:library-entry-filename lib)
                                       #t)))))
              (union (vector->list (hashtable-values comp-table))
                     libs))

    ;; FIXME: Is this necessary?

    (for-each (lambda (lib)
                (hashtable-set! dependency-table lib '()))
              (base-libraries))

    (%compile-stale-libraries2 lib-table
                               comp-table
                               dependency-table)))

;;; Phase 2: calculates dependency graph and then proceeds to phase 3.
;;;
;;; The lib-table maps names of available libraries to library entries.
;;; The comp-table maps names of files to be compiled to library entries.
;;; The dependency table is filled in by phase 2: it maps names of
;;; libraries and programs to lists of the libraries upon which they depend.
;;; For each of those lists, every library depends only upon libraries
;;; that follow it in the list.

(define (%compile-stale-libraries2 lib-table comp-table dependency-table)

  (define (dependencies name)
    (let ((probe (hashtable-ref dependency-table name #f)))
      (cond ((eq? probe #t)
             (error 'compile-stale
                    "import graph contains cycles"
                    name)
             '())
            (probe probe)
            (else
             (hashtable-set! dependency-table name #t)
             (let* ((lib (hashtable-ref lib-table name #f))
                    (lib (or lib (hashtable-ref comp-table name #f))))
               (if (not lib)
                   (begin (display "library not found: ")
                          (write name)
                          (newline)
                          (hashtable-set! dependency-table name '())
                          '())
                   (let ((directly-imported
                          (make-set
                           (map import-spec->libname
                                (cdr (larceny:library-entry-imports lib))))))
                     (let loop ((directly-imported directly-imported)
                                (depends-upon      '()))
                       (if (null? directly-imported)
                           (begin (hashtable-set! dependency-table
                                                  name
                                                  depends-upon)
                                  depends-upon)
                           (let* ((import1 (import-spec->libname
                                            (car directly-imported)))
                                  (depends (dependencies import1)))
                             (loop (cdr directly-imported)
                                   (union (cons import1 depends)
                                          depends-upon))))))))))))

  (vector-for-each dependencies (hashtable-keys lib-table))

  (if (debugging?)
      (vector-for-each (lambda (name)
                         (write name)
                         (newline)
                         (let* ((entry (hashtable-ref lib-table name #f))
                                (entry (or entry
                                           (hashtable-ref comp-table name #f)))
                                (filename
                                 (if entry
                                     (larceny:library-entry-filename entry)
                                     "")))
                           (if (stale? filename)
                               (display " S  ")
                               (display "    "))
                           (write filename)
                           (newline))
                         (for-each (lambda (libname)
                                     (display "    ")
                                     (write libname)
                                     (newline))
                                   (hashtable-ref dependency-table name '())))
                       (hashtable-keys dependency-table)))

  (%compile-stale-libraries3 lib-table comp-table dependency-table))

;;; Phase 3: identifies the files that need to be compiled, and proceeds
;;; to phase 4.  The files that need to be compiled are
;;;
;;;         the named files
;;;         the stale files upon which a named file depends
;;;         the as-yet-uncompiled files upon which a named file depends
;;;         FIXME
;;;
;;; Phase 4 will then check whether any available libraries that are not
;;; in the list of files to be compiled depend upon a file to be compiled.
;;;
;;; The lib-table maps names of available libraries to library entries.
;;; The comp-table maps names of files to be compiled to library entries.
;;; The dependency table maps names of libraries and programs to lists
;;; of the libraries upon which they depend.  For each of those lists,
;;; every library depends only upon libraries that follow it in the list.


(define (%compile-stale-libraries3 lib-table comp-table dependency-table)

  ;; Returns the set of libraries that need to be compiled.

  (define (libraries-to-compile)

    ;; Given a list of entries for files to be compiled,
    ;; returns a list of the stale or as-yet-uncompiled libraries
    ;; on which they depend.

    (define (stale-libraries entries)
;(write (list 'FIXME 'stale-libraries entries)) (newline)
      (if (null? entries)
          '()
          (union (filter (lambda (lib)
                           (let* ((entry (hashtable-ref lib-table lib #f))
                                  (file
                                   (and
                                    entry
                                    (larceny:library-entry-filename entry))))
                             (and file
                                  (or (stale? file)
                                      (not (compiled? file))))))
                         (let* ((entry (car entries))
                                (lib (larceny:library-entry-name entry)))
                           (hashtable-ref dependency-table lib)))
                 (stale-libraries (cdr entries)))))

    ;; Given a list of libraries to be compiled,
    ;; Returns a list of all available libraries that depend upon
    ;; one of the libraries to be compiled.

    (define (libs-to-compile libs0)
      (let loop ((libs (vector->list (hashtable-keys lib-table)))
                 (to-compile libs0))
;(write (list 'FIXME 'libs-to-compile libs to-compile)) (newline)
        (cond ((null? libs)
               to-compile)
              ((let* ((lib (car libs))
                      (prior-libs (hashtable-ref dependency-table lib)))
                 (any (lambda (lib) (member lib libs0))
                      prior-libs))
               (loop (cdr libs)
                     (union (list (car libs)) to-compile)))
              (else
               (loop (cdr libs) to-compile)))))

    (libs-to-compile
     (stale-libraries
      (vector->list
       (hashtable-values comp-table)))))

  ;; Given two sets of libraries to compile and a list of libraries
  ;; that can be compiled in reverse order of the list, returns a
  ;; list containing all libraries in those three arguments, in an
  ;; order that compiles each library only after all libraries on
  ;; which it depends have been compiled.
  ;;
  ;; FIXME: goes into an infinite loop if there are circular dependencies.

  (define (compilation-order to-compile not-ready ready)
;(write (list 'FIXME 'compilation-order to-compile not-ready ready)) (newline)
    (cond ((and (null? to-compile)
                (null? not-ready))
           (reverse ready))
          ((null? to-compile)
           (compilation-order not-ready '() ready))
          ((every (lambda (lib)
                    (and (not (member lib to-compile))
                         (not (member lib not-ready))))
                  (hashtable-ref dependency-table (car to-compile)))
           (compilation-order (cdr to-compile)
                              not-ready
                              (cons (car to-compile) ready)))
          (else
           (compilation-order (cdr to-compile)
                              (cons (car to-compile) not-ready)
                              ready))))

  (let* ((libs (libraries-to-compile))
         (libs (compilation-order libs '() '())))

    (if (debugging?)
        (begin (display "\n\nLibraries to be compiled:\n\n")
               (for-each (lambda (lib)
                           (display "    ")
                           (display lib)
                           (newline))
                         libs)))

    'FIXME

    libs))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


;;; Given lists x and y with no repetitions (in the sense of equal?),
;;; returns a list that's equal to (make-set (append x y)).
;;;
;;; FIXME: unnecessarily inefficient

(define (union x y)
  (make-set (append x y)))

;;; Given a list of objects,
;;; returns that list from which repetitions (in the sense of equal?)
;;; have been eliminated by removing objects earlier in the list
;;; if they are repeated later in the list.

(define (make-set bag)
  (if (null? bag)
      bag
      (let ((set (make-set (cdr bag))))
        (if (member (car bag) set)
            set
            (cons (car bag) set)))))

;; FIXME: hard-wired special treatment for core and primitives isn't right

(define (import-spec->libname spec)
  (cond ((or (not (list? spec))
             (< (length spec) 2))
         spec)
        ((memq (car spec)
               '(only except prefix rename             ; R7RS/R6RS
                 for library))                         ; R6RS only
         (import-spec->libname (cadr spec)))
        ((memq (car spec)
               '(core primitives))                     ; Larceny-specific
         '(rnrs base))
        (else
         (larceny:libname-without-version spec))))

;;; A source file is stale if and only if its associated .slfasl file
;;; is stale.
;;;
;;; A .slfasl file is stale if and only if its source file has
;;; been modified since the .slfasl file was last modified.
;;;
;;; A .slfasl file whose source file cannot be located is not
;;; considered stale.  (Rationale:  This allows libraries to
;;; be supplied in compiled form without their source code.
;;; That's risky, however, because there is no way to recompile
;;; the missing source code if any of the files they depend upon
;;; are recompiled.)

(define (stale? srcfile)
  (let ((faslfile (generate-fasl-name srcfile)))
    (and faslfile
         (file-exists? srcfile)
         (file-exists? faslfile)
         (file-newer? srcfile faslfile))))

(define (compiled? srcfile)
  (let ((faslfile (generate-fasl-name srcfile)))
    (and faslfile
         (file-exists? srcfile)
         (file-exists? faslfile))))

;; These two libraries are defined within r6rs-standard-libraries.sch
;; FIXME: shouldn't need so many special cases

(define (base-libraries)
  '((rnrs base)
    (rnrs io simple)))
