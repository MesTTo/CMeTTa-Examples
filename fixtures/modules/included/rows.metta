;Pasted into the including space by `include`, rather than given a space of
;its own the way an `import!`ed module is. The last directive's answers are
;what the include answers.
(= (included-double $x) (* 2 $x))
(included-fact carried)
!(included-double 21)
