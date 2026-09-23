; A package manifest. `pkg.metta` is the file a DIRECTORY is imported through,
; so an importer names the directory and never an entry point inside it.
(= (package name) greeter)
(= (package version) "0.1.0")

(= (greet $who) (Hello $who))
